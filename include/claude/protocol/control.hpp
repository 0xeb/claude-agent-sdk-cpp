#ifndef CLAUDE_PROTOCOL_CONTROL_HPP
#define CLAUDE_PROTOCOL_CONTROL_HPP

#include <atomic>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>

namespace claude
{

// JSON type alias (also defined in types.hpp)
// Duplicate declaration needed here since control.hpp can't include types.hpp (circular dependency)
using json = nlohmann::json;

namespace protocol
{

// Control request - sent from SDK to CLI
struct ControlRequest
{
    std::string type = "control_request";
    std::string request_id;
    json request; // Subtype-specific data
};

// Control response - received from CLI
struct ControlResponse
{
    std::string type = "control_response";
    struct Response
    {
        std::string subtype; // "success" or "error"
        std::string request_id;
        json response; // Response data
        std::string error;       // Error message if failed
    } response;
};

// Control protocol manager - handles async request/response correlation
class ControlProtocol
{
  public:
    ControlProtocol();
    ~ControlProtocol();

    // No copy
    ControlProtocol(const ControlProtocol&) = delete;
    ControlProtocol& operator=(const ControlProtocol&) = delete;

    // Send control request and wait for response
    // Returns response data on success, throws on error/timeout
    json send_request(const std::function<void(const std::string&)>& write_func,
                                const std::string& subtype, const json& request_data,
                                int timeout_ms = 60000);

    // Handle incoming control response
    void handle_response(const ControlResponse& response);

    // Generate unique request ID
    std::string generate_request_id();

    // Build control request message JSON string
    std::string build_request_message(const std::string& subtype, const json& data);

  private:
    std::atomic<int> request_counter_{0};

    // Pending requests - maps request_id to promise
    std::map<std::string, std::promise<json>> pending_requests_;
    std::mutex requests_mutex_;

    // Register a pending request and return future
    std::future<json> register_request(const std::string& request_id);

    // Resolve a pending request with success
    void resolve_request(const std::string& request_id, const json& data);

    // Reject a pending request with error
    void reject_request(const std::string& request_id, const std::string& error);
};

} // namespace protocol
} // namespace claude

#endif // CLAUDE_PROTOCOL_CONTROL_HPP
