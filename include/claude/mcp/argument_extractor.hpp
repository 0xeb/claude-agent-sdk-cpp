#ifndef CLAUDE_MCP_ARGUMENT_EXTRACTOR_HPP
#define CLAUDE_MCP_ARGUMENT_EXTRACTOR_HPP

#include <array>
#include <claude/mcp/type_traits.hpp>
#include <functional>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace claude
{
namespace mcp
{

// ============================================================================
// JSON to Value Conversion
// ============================================================================

/// Convert JSON value to C++ type with error handling
template <typename T>
T extract_json_value(const json& j, const std::string& param_name)
{
    using BaseType = remove_cvref_t<T>;

    try
    {
        if constexpr (std::is_same_v<BaseType, json>)
        {
            // Pass JSON through as-is
            return j;
        }
        else if constexpr (std::is_same_v<BaseType, std::string>)
        {
            if (j.is_string())
                return j.get<std::string>();
            // Try to convert other types to string
            return j.dump();
        }
        else
        {
            // Use nlohmann::json's built-in conversion
            return j.get<BaseType>();
        }
    }
    catch (const json::exception& e)
    {
        throw std::runtime_error("Failed to extract parameter '" + param_name +
                                 "': " + std::string(e.what()));
    }
}

// ============================================================================
// Argument Extraction and Function Invocation
// ============================================================================

/// Extract arguments from JSON and invoke function (with indices)
template <typename Func, typename... Args, size_t... Indices>
auto invoke_with_json_impl(Func& func, const json& args,
                           const std::array<std::string, sizeof...(Args)>& param_names,
                           std::index_sequence<Indices...>)
{
    // Extract each argument by name and invoke the function
    return func(extract_json_value<Args>(args.at(param_names[Indices]), param_names[Indices])...);
}

/// Extract arguments from JSON and invoke function (public interface)
template <typename Func, typename... Args>
auto invoke_with_json(Func& func, const json& args,
                      const std::array<std::string, sizeof...(Args)>& param_names)
{
    return invoke_with_json_impl<Func, Args...>(func, args, param_names,
                                                std::make_index_sequence<sizeof...(Args)>{});
}

// ============================================================================
// JSON Invoker - Type-erased function wrapper with JSON I/O
// ============================================================================

/// Wraps a function to accept JSON input and return JSON output
template <typename Func>
class JsonInvoker
{
  public:
    using Traits = FunctionTraits<remove_cvref_t<Func>>;
    using ReturnType = typename Traits::ReturnType;
    static constexpr size_t Arity = Traits::arity;

    /// Construct with function and parameter names
    JsonInvoker(const Func& f, std::array<std::string, Arity> names)
        : func_(f), param_names_(std::move(names))
    {
    }

    /// Invoke function with JSON arguments, return JSON result
    json operator()(const json& args)
    {
        return invoke_and_wrap(args, std::make_index_sequence<Arity>{});
    }

  private:
    /// Extract arguments, invoke function, and wrap result (with indices)
    template <size_t... Indices>
    json invoke_and_wrap(const json& args, std::index_sequence<Indices...>)
    {
        using ArgsTuple = typename Traits::ArgsTuple;

        // Extract each argument type
        using ArgTypes = std::tuple<std::tuple_element_t<Indices, ArgsTuple>...>;

        // Invoke with extracted arguments
        if constexpr (std::is_same_v<ReturnType, void>)
        {
            // Void return - call and return success message
            invoke_with_json_impl<Func, std::tuple_element_t<Indices, ArgsTuple>...>(
                func_, args, param_names_, std::index_sequence<Indices...>{});

            return wrap_void_result();
        }
        else
        {
            // Non-void return - call and wrap result
            auto result = invoke_with_json_impl<Func, std::tuple_element_t<Indices, ArgsTuple>...>(
                func_, args, param_names_, std::index_sequence<Indices...>{});

            return wrap_result(result);
        }
    }

    /// Wrap void result in MCP format
    json wrap_void_result()
    {
        return json{{"content", json::array({json{{"type", "text"}, {"text", "Success"}}})}};
    }

    /// Wrap result in MCP format
    template <typename T>
    json wrap_result(const T& result)
    {
        using BaseType = remove_cvref_t<T>;

        if constexpr (std::is_same_v<BaseType, json>)
        {
            // If result is already JSON, check if it's MCP-formatted
            if (result.contains("content"))
            {
                // Already MCP-formatted
                return result;
            }
            else
            {
                // Raw JSON - wrap in MCP format
                return json{
                    {"content", json::array({json{{"type", "text"}, {"text", result.dump()}}})}};
            }
        }
        else if constexpr (std::is_same_v<BaseType, std::string>)
        {
            // String result
            return json{{"content", json::array({json{{"type", "text"}, {"text", result}}})}};
        }
        else if constexpr (std::is_arithmetic_v<BaseType>)
        {
            // Numeric result - convert to string
            return json{{"content",
                         json::array({json{{"type", "text"}, {"text", std::to_string(result)}}})}};
        }
        else if constexpr (std::is_same_v<BaseType, bool>)
        {
            // Boolean result - convert to string
            return json{{"content", json::array({json{{"type", "text"},
                                                      {"text", result ? "true" : "false"}}})}};
        }
        else
        {
            // Try to convert to JSON and wrap
            json j = result;
            return json{{"content", json::array({json{{"type", "text"}, {"text", j.dump()}}})}};
        }
    }

    Func func_;
    std::array<std::string, Arity> param_names_;
};

// ============================================================================
// Factory Function
// ============================================================================

/// Create a JsonInvoker from a function and parameter names
template <typename Func>
auto make_json_invoker(Func&& func, std::vector<std::string> names)
{
    using Traits = FunctionTraits<remove_cvref_t<Func>>;
    static constexpr size_t Arity = Traits::arity;

    // Validate parameter count
    if (names.size() != Arity)
    {
        throw std::invalid_argument("Parameter name count mismatch: expected " +
                                    std::to_string(Arity) + ", got " +
                                    std::to_string(names.size()));
    }

    // Convert vector to array
    std::array<std::string, Arity> names_array;
    for (size_t i = 0; i < Arity; ++i)
        names_array[i] = names[i];

    return JsonInvoker<remove_cvref_t<Func>>(func, std::move(names_array));
}

} // namespace mcp
} // namespace claude

#endif // CLAUDE_MCP_ARGUMENT_EXTRACTOR_HPP
