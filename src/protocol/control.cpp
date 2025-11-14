#include <chrono>
#include <claude/errors.hpp>
#include <claude/protocol/control.hpp>
#include <iomanip>
#include <random>
#include <sstream>

namespace claude
{
namespace protocol
{

ControlProtocol::ControlProtocol() {}

ControlProtocol::~ControlProtocol()
{
    // Cancel all pending requests
    std::lock_guard<std::mutex> lock(requests_mutex_);
    for (auto& [id, promise] : pending_requests_)
    {
        try
        {
            promise.set_exception(
                std::make_exception_ptr(ClaudeError("Control protocol shutting down")));
        }
        catch (...)
        {
            // Promise already satisfied
        }
    }
    pending_requests_.clear();
}

std::string ControlProtocol::generate_request_id()
{
    // Generate: req_{counter}_{random}
    int counter = request_counter_++;

    // Random hex string (4 bytes = 8 hex chars)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    std::ostringstream oss;
    oss << "req_" << counter << "_";
    for (int i = 0; i < 4; ++i)
    {
        oss << std::hex << std::setfill('0') << std::setw(2) << dis(gen);
    }

    return oss.str();
}

std::string ControlProtocol::build_request_message(const std::string& subtype,
                                                   const json& data)
{
    std::string request_id = generate_request_id();

    json request = data;
    request["subtype"] = subtype;

    json msg = {
        {"type", "control_request"}, {"request_id", request_id}, {"request", request}};

    return msg.dump() + "\n";
}

std::future<json> ControlProtocol::register_request(const std::string& request_id)
{
    std::lock_guard<std::mutex> lock(requests_mutex_);

    std::promise<json> promise;
    auto future = promise.get_future();

    pending_requests_[request_id] = std::move(promise);

    return future;
}

json
ControlProtocol::send_request(const std::function<void(const std::string&)>& write_func,
                              const std::string& subtype, const json& request_data,
                              int timeout_ms)
{
    std::string request_id = generate_request_id();

    // Build message
    json request = request_data;
    request["subtype"] = subtype;

    json msg = {
        {"type", "control_request"}, {"request_id", request_id}, {"request", request}};

    // Register pending request BEFORE sending
    auto future = register_request(request_id);

    // Send message via write function
    std::string json_str = msg.dump() + "\n";
    write_func(json_str);

    // Wait for response with timeout
    if (timeout_ms > 0)
    {
        auto status = future.wait_for(std::chrono::milliseconds(timeout_ms));
        if (status == std::future_status::timeout)
        {
            // Remove from pending
            {
                std::lock_guard<std::mutex> lock(requests_mutex_);
                pending_requests_.erase(request_id);
            }
            throw ClaudeError("Control request timed out: " + subtype);
        }
    }

    return future.get();
}

void ControlProtocol::handle_response(const ControlResponse& response)
{
    const auto& resp = response.response;

    if (resp.subtype == "success")
    {
        resolve_request(resp.request_id, resp.response);
    }
    else if (resp.subtype == "error")
    {
        reject_request(resp.request_id, resp.error);
    }
    else
    {
        // Unknown subtype, reject
        reject_request(resp.request_id, "Unknown response subtype: " + resp.subtype);
    }
}

void ControlProtocol::resolve_request(const std::string& request_id, const json& data)
{
    std::lock_guard<std::mutex> lock(requests_mutex_);

    auto it = pending_requests_.find(request_id);
    if (it != pending_requests_.end())
    {
        try
        {
            it->second.set_value(data);
        }
        catch (...)
        {
            // Promise already satisfied
        }
        pending_requests_.erase(it);
    }
}

void ControlProtocol::reject_request(const std::string& request_id, const std::string& error)
{
    std::lock_guard<std::mutex> lock(requests_mutex_);

    auto it = pending_requests_.find(request_id);
    if (it != pending_requests_.end())
    {
        try
        {
            it->second.set_exception(std::make_exception_ptr(ClaudeError(error)));
        }
        catch (...)
        {
            // Promise already satisfied
        }
        pending_requests_.erase(it);
    }
}

} // namespace protocol
} // namespace claude
