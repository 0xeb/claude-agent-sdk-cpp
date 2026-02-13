#ifndef CLAUDE_MCP_SERVER_HPP
#define CLAUDE_MCP_SERVER_HPP

#include <claude/mcp/tool.hpp>
#include <functional>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <type_traits>
#include <vector>

namespace claude
{
namespace mcp
{

// ============================================================================
// MCP Tool Annotations (matches Python SDK v0.1.35)
// ============================================================================

/// Provides hints about tool behavior to the model/client.
struct ToolAnnotations
{
    std::optional<std::string> title = std::nullopt;
    std::optional<bool> read_only_hint = std::nullopt;
    std::optional<bool> destructive_hint = std::nullopt;
    std::optional<bool> idempotent_hint = std::nullopt;
    std::optional<bool> open_world_hint = std::nullopt;

    json to_json() const
    {
        json out = json::object();
        if (title.has_value())
            out["title"] = *title;
        if (read_only_hint.has_value())
            out["readOnlyHint"] = *read_only_hint;
        if (destructive_hint.has_value())
            out["destructiveHint"] = *destructive_hint;
        if (idempotent_hint.has_value())
            out["idempotentHint"] = *idempotent_hint;
        if (open_world_hint.has_value())
            out["openWorldHint"] = *open_world_hint;
        return out;
    }

    bool has_any() const
    {
        return title.has_value() || read_only_hint.has_value() || destructive_hint.has_value() ||
               idempotent_hint.has_value() || open_world_hint.has_value();
    }
};

// ============================================================================
// Tool Storage - Type-erased tool registry
// ============================================================================

/// Internal storage for type-erased tools
class ToolStorage
{
  public:
    template <typename U>
    static std::function<json(const json&)> make_handler_from(U&& tool)
    {
        using Decayed = std::decay_t<U>;
        if constexpr (std::is_same_v<Decayed, Tool>)
        {
            auto inv = tool.invoker();
            return [inv = std::move(inv)](const json& args) mutable { return inv(args); };
        }
        else
        {
            return [w = std::forward<U>(tool)](const json& args) mutable { return w.invoke(args); };
        }
    }
    /// Add a tool to the storage
    template <typename ToolWrapper>
    void add(ToolWrapper&& tool)
    {
        add_with_annotations(std::forward<ToolWrapper>(tool), std::nullopt);
    }

    /// Add a tool to the storage with optional annotations
    template <typename ToolWrapper>
    void add_with_annotations(ToolWrapper&& tool, std::optional<ToolAnnotations> annotations)
    {
        std::string tool_name = tool.name();

        // Check for duplicate names
        if (tools_.find(tool_name) != tools_.end())
            throw std::invalid_argument("Duplicate tool name: " + tool_name);

        // Store tool metadata and handler
        auto desc = tool.description();
        auto in_schema = tool.input_schema();
        auto out_schema = tool.output_schema();
        StoredTool stored{tool_name, std::move(desc), std::move(in_schema), std::move(out_schema),
                          make_handler_from(std::forward<ToolWrapper>(tool)),
                          std::move(annotations)};

        tools_[tool_name] = std::move(stored);
    }

    /// Build an MCP JSON-RPC handler from stored tools
    std::function<json(const json&)> build_handler(const std::string& server_name,
                                                   const std::string& version) const
    {
        // Capture tools by value
        return [server_name, version, tools = tools_](const json& request) -> json
        {
            // Validate request structure
            if (!request.contains("method"))
            {
                return build_error_response(request.value("id", json()), -32600,
                                            "Invalid Request: missing 'method' field");
            }

            std::string method = request["method"];
            json id = request.value("id", json());

            // Route to appropriate handler
            if (method == "initialize")
            {
                return build_initialize_response(id, server_name, version);
            }
            else if (method == "tools/list")
            {
                return build_tools_list_response(id, tools);
            }
            else if (method == "tools/call")
            {
                if (!request.contains("params"))
                {
                    return build_error_response(id, -32600,
                                                "Invalid Request: missing 'params' field");
                }
                return build_tool_call_response(id, request["params"], tools);
            }
            else
            {
                return build_error_response(id, -32601, "Method not found: " + method);
            }
        };
    }

    /// Get number of tools
    size_t size() const
    {
        return tools_.size();
    }

    /// Check if a tool exists
    bool has_tool(const std::string& name) const
    {
        return tools_.find(name) != tools_.end();
    }

  private:
    /// Stored tool information
    struct StoredTool
    {
        std::string name;
        std::string description;
        json input_schema;
        json output_schema;
        std::function<json(const json&)> handler;
        std::optional<ToolAnnotations> annotations = std::nullopt;
    };

    std::map<std::string, StoredTool> tools_;

    /// Build initialize response
    static json build_initialize_response(const json& id, const std::string& server_name,
                                          const std::string& version)
    {
        return json{
            {"jsonrpc", "2.0"},
            {"id", id},
            {"result",
             {{"protocolVersion", "2024-11-05"},
              {"serverInfo", {{"name", server_name}, {"version", version}}},
              {"capabilities", {{"tools", json::object()}, {"resources", json::object()}}}}}};
    }

    /// Build tools/list response
    static json build_tools_list_response(const json& id,
                                          const std::map<std::string, StoredTool>& tools)
    {
        json tools_array = json::array();

        for (const auto& [name, tool] : tools)
        {
            json tool_obj = {{"name", tool.name},
                             {"description", tool.description},
                             {"inputSchema", tool.input_schema}};
            if (tool.annotations.has_value() && tool.annotations->has_any())
                tool_obj["annotations"] = tool.annotations->to_json();
            tools_array.push_back(tool_obj);
        }

        return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", {{"tools", tools_array}}}};
    }

    /// Build tools/call response
    static json build_tool_call_response(const json& id, const json& params,
                                         const std::map<std::string, StoredTool>& tools)
    {
        try
        {
            // Extract tool name and arguments
            if (!params.contains("name"))
                return build_error_response(id, -32602, "Invalid params: missing 'name' field");

            std::string tool_name = params["name"];
            json arguments = params.value("arguments", json::object());

            // Find tool
            auto it = tools.find(tool_name);
            if (it == tools.end())
                return build_error_response(id, -32602, "Tool not found: " + tool_name);

            // Invoke tool
            json result = it->second.handler(arguments);

            // Return result
            return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
        }
        catch (const std::exception& e)
        {
            return build_error_response(id, -32603, "Internal error: " + std::string(e.what()));
        }
    }

    /// Build error response
    static json build_error_response(const json& id, int code, const std::string& message)
    {
        return json{
            {"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", message}}}};
    }
};

// ============================================================================
// Server Factory Functions
// ============================================================================

/// Create an MCP server handler from variadic tools
template <typename... Tools>
auto create_server(const std::string& name, const std::string& version, Tools&&... tools)
{
    ToolStorage storage;

    // Add all tools using fold expression (C++17)
    (storage.add(std::forward<Tools>(tools)), ...);

    // Build and return handler
    return storage.build_handler(name, version);
}

/// Create an MCP server handler from a vector of type-erased tools
inline auto create_server(const std::string& name, const std::string& version,
                          std::vector<Tool> tools)
{
    ToolStorage storage;

    for (auto& tool : tools)
        storage.add(std::move(tool));

    return storage.build_handler(name, version);
}

// ============================================================================
// Server Builder (fluent interface for complex setups)
// ============================================================================

/// Fluent builder for MCP servers
class ServerBuilder
{
  public:
    /// Construct with name and version
    ServerBuilder(std::string name, std::string version)
        : name_(std::move(name)), version_(std::move(version))
    {
    }

    /// Add a tool to the server
    template <typename ToolWrapper>
    ServerBuilder& add_tool(ToolWrapper&& tool)
    {
        storage_.add(std::forward<ToolWrapper>(tool));
        return *this;
    }

    /// Add a tool to the server with annotations
    template <typename ToolWrapper>
    ServerBuilder& add_tool(ToolWrapper&& tool, ToolAnnotations annotations)
    {
        storage_.add_with_annotations(std::forward<ToolWrapper>(tool), std::move(annotations));
        return *this;
    }

    /// Build the server handler
    std::function<json(const json&)> build()
    {
        return storage_.build_handler(name_, version_);
    }

    /// Get number of registered tools
    size_t tool_count() const
    {
        return storage_.size();
    }

  private:
    std::string name_;
    std::string version_;
    ToolStorage storage_;
};

/// Create a server builder
inline ServerBuilder server(const std::string& name, const std::string& version)
{
    return ServerBuilder(name, version);
}

} // namespace mcp
} // namespace claude

#endif // CLAUDE_MCP_SERVER_HPP
