#include <claude/sessions/session_summary.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <regex>
#include <sstream>
#include <string>

namespace claude::sessions
{

namespace
{

// Pattern matching auto-generated / system messages that should be skipped
// when looking for the first meaningful user prompt. Python: sessions.py:51
// _SKIP_FIRST_PROMPT_PATTERN.
const std::regex& skip_first_prompt_pattern()
{
    static const std::regex re(
        R"(^(?:<local-command-stdout>|<session-start-hook>|<tick>|<goal>|\[Request interrupted by user[^\]]*\]|\s*<ide_opened_file>[\s\S]*</ide_opened_file>\s*$|\s*<ide_selection>[\s\S]*</ide_selection>\s*$))");
    return re;
}

// Python: sessions.py:58 _COMMAND_NAME_RE
const std::regex& command_name_re()
{
    static const std::regex re(R"(<command-name>(.*?)</command-name>)");
    return re;
}

// Map of JSONL keys -> SessionSummaryEntry data keys for last-wins string fields.
// Python: session_summary.py:30 _LAST_WINS_FIELDS.
const std::vector<std::pair<std::string, std::string>>& last_wins_fields()
{
    static const std::vector<std::pair<std::string, std::string>> v = {
        {"customTitle", "custom_title"},
        {"aiTitle", "ai_title"},
        {"lastPrompt", "last_prompt"},
        {"summary", "summary_hint"},
        {"gitBranch", "git_branch"},
    };
    return v;
}

// Helper: trim trailing whitespace from a string.
std::string rstrip(std::string s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.pop_back();
    return s;
}

// Helper: strip leading + trailing whitespace.
std::string strip(const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a])))
        ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])))
        --b;
    return s.substr(a, b - a);
}

// Replace newlines with single spaces (Python: raw.replace("\n", " ")).
std::string replace_newlines(std::string s)
{
    for (auto& c : s)
        if (c == '\n')
            c = ' ';
    return s;
}

// Truncate to 200 chars with horizontal ellipsis (matches Python "result[:200].rstrip() + '\u2026'").
// "Chars" here means bytes (the Python code is similarly byte-based for ASCII-heavy prompts;
// UTF-8 truncation may split a multibyte sequence — caller treats as best-effort display).
std::string maybe_truncate_200(const std::string& s)
{
    if (s.size() <= 200)
        return s;
    // u2026 = horizontal ellipsis = E2 80 A6
    return rstrip(s.substr(0, 200)) + "\xE2\x80\xA6";
}

// Pull text strings from an entry's "message.content" field.
// Python: session_summary.py:_entry_text_blocks (line 51).
std::vector<std::string> entry_text_blocks(const json& entry)
{
    std::vector<std::string> out;
    if (!entry.is_object())
        return out;
    auto it = entry.find("message");
    if (it == entry.end() || !it->is_object())
        return out;
    auto cit = it->find("content");
    if (cit == it->end())
        return out;
    if (cit->is_string())
    {
        out.push_back(cit->get<std::string>());
    }
    else if (cit->is_array())
    {
        for (const auto& block : *cit)
        {
            if (!block.is_object())
                continue;
            auto tit = block.find("type");
            if (tit == block.end() || !tit->is_string() || tit->get<std::string>() != "text")
                continue;
            auto txt = block.find("text");
            if (txt != block.end() && txt->is_string())
                out.push_back(txt->get<std::string>());
        }
    }
    return out;
}

// Python: session_summary.py:_fold_first_prompt (line 71).
void fold_first_prompt(json& data, const json& entry)
{
    auto bool_field = [&](const json& obj, const char* key) {
        auto it = obj.find(key);
        return it != obj.end() && it->is_boolean() && it->get<bool>();
    };

    if (data.value("first_prompt_locked", false))
        return;
    if (!entry.is_object())
        return;
    auto tit = entry.find("type");
    if (tit == entry.end() || !tit->is_string() || tit->get<std::string>() != "user")
        return;
    if (bool_field(entry, "isMeta") || bool_field(entry, "isCompactSummary"))
        return;

    // Skip tool_result-carrying user messages.
    auto mit = entry.find("message");
    if (mit != entry.end() && mit->is_object())
    {
        auto cit = mit->find("content");
        if (cit != mit->end() && cit->is_array())
        {
            for (const auto& b : *cit)
            {
                if (b.is_object())
                {
                    auto bt = b.find("type");
                    if (bt != b.end() && bt->is_string() && bt->get<std::string>() == "tool_result")
                        return;
                }
            }
        }
    }

    for (const auto& raw : entry_text_blocks(entry))
    {
        std::string result = strip(replace_newlines(raw));
        if (result.empty())
            continue;

        std::smatch m;
        if (std::regex_search(result, m, command_name_re()))
        {
            if (!data.contains("command_fallback") || !data["command_fallback"].is_string() ||
                data["command_fallback"].get<std::string>().empty())
                data["command_fallback"] = m[1].str();
            continue;
        }
        if (std::regex_search(result, skip_first_prompt_pattern()))
            continue;

        result = maybe_truncate_200(result);
        data["first_prompt"] = result;
        data["first_prompt_locked"] = true;
        return;
    }
}

} // namespace

std::optional<int64_t> iso_to_epoch_ms(const std::string& ts)
{
    if (ts.empty())
        return std::nullopt;
    // Accept "...Z" or "...+HH:MM" or "...-HH:MM" or no tz (assume UTC).
    // We parse: YYYY-MM-DDTHH:MM:SS(.fraction)?(Z|+HH:MM|-HH:MM)?
    std::tm tm{};
    int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0;
    int frac_ms = 0;
    int tz_offset_minutes = 0;
    const char* s = ts.c_str();
    int matched = std::sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day, &hour, &min, &sec);
    if (matched < 6)
        return std::nullopt;
    // Locate position after seconds (always at offset 19).
    size_t pos = 19;
    if (pos < ts.size() && ts[pos] == '.')
    {
        ++pos;
        std::string frac;
        while (pos < ts.size() && std::isdigit(static_cast<unsigned char>(ts[pos])))
        {
            frac.push_back(ts[pos]);
            ++pos;
        }
        // Pad/truncate to 3 chars for milliseconds.
        while (frac.size() < 3)
            frac.push_back('0');
        frac = frac.substr(0, 3);
        try
        {
            frac_ms = std::stoi(frac);
        }
        catch (...)
        {
            frac_ms = 0;
        }
    }
    if (pos < ts.size())
    {
        if (ts[pos] == 'Z')
        {
            tz_offset_minutes = 0;
        }
        else if (ts[pos] == '+' || ts[pos] == '-')
        {
            int sign = ts[pos] == '+' ? 1 : -1;
            int th = 0, tm2 = 0;
            if (std::sscanf(s + pos + 1, "%2d:%2d", &th, &tm2) == 2)
            {
                tz_offset_minutes = sign * (th * 60 + tm2);
            }
            else
            {
                return std::nullopt;
            }
        }
    }
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_sec = sec;
#ifdef _WIN32
    std::time_t t_utc = _mkgmtime(&tm);
#else
    std::time_t t_utc = timegm(&tm);
#endif
    if (t_utc == static_cast<std::time_t>(-1))
        return std::nullopt;
    int64_t ms = static_cast<int64_t>(t_utc) * 1000 + frac_ms - tz_offset_minutes * 60 * 1000;
    return ms;
}

std::string extract_first_prompt_from_head(const std::string& head)
{
    size_t start = 0;
    std::string command_fallback;
    size_t head_len = head.size();

    while (start < head_len)
    {
        size_t nl = head.find('\n', start);
        std::string line;
        if (nl != std::string::npos)
        {
            line = head.substr(start, nl - start);
            start = nl + 1;
        }
        else
        {
            line = head.substr(start);
            start = head_len;
        }

        // Cheap text guards before full parse (Python optimization).
        if (line.find("\"type\":\"user\"") == std::string::npos &&
            line.find("\"type\": \"user\"") == std::string::npos)
            continue;
        if (line.find("\"tool_result\"") != std::string::npos)
            continue;
        if (line.find("\"isMeta\":true") != std::string::npos ||
            line.find("\"isMeta\": true") != std::string::npos)
            continue;
        if (line.find("\"isCompactSummary\":true") != std::string::npos ||
            line.find("\"isCompactSummary\": true") != std::string::npos)
            continue;

        json entry;
        try
        {
            entry = json::parse(line);
        }
        catch (...)
        {
            continue;
        }
        if (!entry.is_object())
            continue;
        auto tit = entry.find("type");
        if (tit == entry.end() || !tit->is_string() || tit->get<std::string>() != "user")
            continue;

        for (const auto& raw : entry_text_blocks(entry))
        {
            std::string result = strip(replace_newlines(raw));
            if (result.empty())
                continue;
            std::smatch m;
            if (std::regex_search(result, m, command_name_re()))
            {
                if (command_fallback.empty())
                    command_fallback = m[1].str();
                continue;
            }
            if (std::regex_search(result, skip_first_prompt_pattern()))
                continue;
            return maybe_truncate_200(result);
        }
    }
    return command_fallback;
}

SessionSummaryEntry fold_session_summary(const std::optional<SessionSummaryEntry>& prev,
                                         const SessionKey& key,
                                         const std::vector<SessionStoreEntry>& entries)
{
    SessionSummaryEntry summary;
    if (prev.has_value())
    {
        summary.session_id = prev->session_id;
        summary.mtime = prev->mtime;
        summary.data = prev->data; // deep copy
    }
    else
    {
        summary.session_id = key.session_id;
        summary.mtime = 0;
        summary.data = json::object();
    }
    if (!summary.data.is_object())
        summary.data = json::object();
    json& data = summary.data;

    for (const auto& entry : entries)
    {
        if (!entry.is_object())
            continue;

        std::optional<int64_t> ms_opt;
        auto tsit = entry.find("timestamp");
        if (tsit != entry.end() && tsit->is_string())
            ms_opt = iso_to_epoch_ms(tsit->get<std::string>());

        if (!data.contains("is_sidechain"))
        {
            auto sit = entry.find("isSidechain");
            data["is_sidechain"] =
                sit != entry.end() && sit->is_boolean() && sit->get<bool>();
        }
        if (!data.contains("created_at") && ms_opt.has_value())
            data["created_at"] = *ms_opt;

        if (!data.contains("cwd"))
        {
            auto cwit = entry.find("cwd");
            if (cwit != entry.end() && cwit->is_string() && !cwit->get<std::string>().empty())
                data["cwd"] = cwit->get<std::string>();
        }

        fold_first_prompt(data, entry);

        for (const auto& kv : last_wins_fields())
        {
            auto it = entry.find(kv.first);
            if (it != entry.end() && it->is_string())
                data[kv.second] = it->get<std::string>();
        }

        auto tit = entry.find("type");
        if (tit != entry.end() && tit->is_string() && tit->get<std::string>() == "tag")
        {
            auto tag_it = entry.find("tag");
            if (tag_it != entry.end() && tag_it->is_string() &&
                !tag_it->get<std::string>().empty())
            {
                data["tag"] = tag_it->get<std::string>();
            }
            else
            {
                data.erase("tag");
            }
        }
    }

    (void)key; // session_id already captured from key above
    return summary;
}

std::optional<SDKSessionInfo>
summary_entry_to_sdk_info(const SessionSummaryEntry& entry,
                          const std::optional<std::string>& project_path)
{
    const json& data = entry.data;
    if (!data.is_object())
        return std::nullopt;
    if (data.value("is_sidechain", false))
        return std::nullopt;

    auto get_str = [&](const char* k) -> std::optional<std::string> {
        auto it = data.find(k);
        if (it != data.end() && it->is_string() && !it->get<std::string>().empty())
            return it->get<std::string>();
        return std::nullopt;
    };

    std::optional<std::string> first_prompt;
    if (data.value("first_prompt_locked", false))
        first_prompt = get_str("first_prompt");
    else
        first_prompt = get_str("command_fallback");

    auto custom_title = get_str("custom_title");
    if (!custom_title)
        custom_title = get_str("ai_title");

    std::optional<std::string> summary = custom_title;
    if (!summary)
        summary = get_str("last_prompt");
    if (!summary)
        summary = get_str("summary_hint");
    if (!summary)
        summary = first_prompt;
    if (!summary)
        return std::nullopt;

    SDKSessionInfo info;
    info.session_id = entry.session_id;
    info.summary = *summary;
    info.last_modified = entry.mtime;
    info.file_size = std::nullopt;
    info.custom_title = custom_title;
    info.first_prompt = first_prompt;
    info.git_branch = get_str("git_branch");
    auto cwd = get_str("cwd");
    if (!cwd)
        cwd = project_path;
    info.cwd = cwd;
    info.tag = get_str("tag");
    auto cit = data.find("created_at");
    if (cit != data.end() && cit->is_number_integer())
        info.created_at = cit->get<int64_t>();
    return info;
}

} // namespace claude::sessions
