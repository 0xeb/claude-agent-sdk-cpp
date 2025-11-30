#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace claude_agent_sdk::internal
{

/// Compute SHA256 hash of a file
/// Returns hex-encoded SHA256 hash (64 characters) or std::nullopt on error
std::optional<std::string> compute_file_sha256(const std::filesystem::path& file_path);

/// Verify CLI path is in the allowlist
/// If allowlist is empty, returns true (no restriction)
/// Otherwise, path must be in the allowlist
bool verify_cli_path_allowed(const std::string& cli_path,
                             const std::vector<std::string>& allowed_paths);

/// Verify CLI hash matches expected SHA256
/// If expected_hash is nullopt, returns true (no hash check)
/// Otherwise, computes file hash and compares
bool verify_cli_hash(const std::filesystem::path& cli_path,
                     const std::optional<std::string>& expected_hash, std::string& error_message);

} // namespace claude_agent_sdk::internal
