#include "cli_verification.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
// clang-format off
#include <windows.h>  // Must be included before wincrypt.h
#include <wincrypt.h>
// clang-format on
#else
#include <openssl/sha.h>
#endif

namespace claude_agent_sdk::internal
{

std::optional<std::string> compute_file_sha256(const std::filesystem::path& file_path)
{
    try
    {
        std::ifstream file(file_path, std::ios::binary);
        if (!file)
            return std::nullopt;

#ifdef _WIN32
        // Windows CryptoAPI
        HCRYPTPROV hProv = 0;
        HCRYPTHASH hHash = 0;

        if (!CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
            return std::nullopt;

        if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash))
        {
            CryptReleaseContext(hProv, 0);
            return std::nullopt;
        }

        const size_t BUFFER_SIZE = 8192;
        char buffer[BUFFER_SIZE];
        while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0)
        {
            if (!CryptHashData(hHash, reinterpret_cast<BYTE*>(buffer),
                               static_cast<DWORD>(file.gcount()), 0))
            {
                CryptDestroyHash(hHash);
                CryptReleaseContext(hProv, 0);
                return std::nullopt;
            }
        }

        BYTE hash[32];
        DWORD hashLen = sizeof(hash);
        if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0))
        {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return std::nullopt;
        }

        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);

        // Convert to hex string
        std::ostringstream oss;
        for (size_t i = 0; i < hashLen; ++i)
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);

        return oss.str();

#else
        // Unix/Linux using OpenSSL
        SHA256_CTX sha256;
        if (!SHA256_Init(&sha256))
            return std::nullopt;

        const size_t BUFFER_SIZE = 8192;
        char buffer[BUFFER_SIZE];
        while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0)
            if (!SHA256_Update(&sha256, buffer, file.gcount()))
                return std::nullopt;

        unsigned char hash[SHA256_DIGEST_LENGTH];
        if (!SHA256_Final(hash, &sha256))
            return std::nullopt;

        // Convert to hex string
        std::ostringstream oss;
        for (size_t i = 0; i < SHA256_DIGEST_LENGTH; ++i)
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);

        return oss.str();
#endif
    }
    catch (...)
    {
        return std::nullopt;
    }
}

bool verify_cli_path_allowed(const std::string& cli_path,
                             const std::vector<std::string>& allowed_paths)
{
    // If no allowlist specified, allow all paths
    if (allowed_paths.empty())
        return true;

    // Normalize both the cli_path and allowed paths for comparison
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path normalized_cli = fs::canonical(cli_path, ec);
    if (ec)
    {
        // If can't normalize, do string comparison
        normalized_cli = fs::path(cli_path);
    }

    // Check if CLI path is in the allowlist
    for (const auto& allowed : allowed_paths)
    {
        fs::path normalized_allowed = fs::canonical(allowed, ec);
        if (ec)
            normalized_allowed = fs::path(allowed);

        if (normalized_cli == normalized_allowed)
            return true;
    }

    return false;
}

bool verify_cli_hash(const std::filesystem::path& cli_path,
                     const std::optional<std::string>& expected_hash, std::string& error_message)
{
    // If no hash check requested, always pass
    if (!expected_hash)
        return true;

    // Validate hash format (should be 64 hex characters for SHA256)
    if (expected_hash->length() != 64)
    {
        error_message = "Invalid hash format: expected 64-character hex string";
        return false;
    }

    // Check all characters are valid hex
    for (char c : *expected_hash)
    {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
        {
            error_message = "Invalid hash format: contains non-hex characters";
            return false;
        }
    }

    // Compute actual file hash
    auto actual_hash = compute_file_sha256(cli_path);
    if (!actual_hash)
    {
        error_message = "Failed to compute file hash";
        return false;
    }

    // Compare hashes (case-insensitive)
    std::string expected_lower = *expected_hash;
    std::string actual_lower = *actual_hash;
    std::transform(expected_lower.begin(), expected_lower.end(), expected_lower.begin(), ::tolower);
    std::transform(actual_lower.begin(), actual_lower.end(), actual_lower.begin(), ::tolower);

    if (expected_lower != actual_lower)
    {
        error_message =
            "CLI hash mismatch: expected " + expected_lower + " but got " + actual_lower;
        return false;
    }

    return true;
}

} // namespace claude_agent_sdk::internal
