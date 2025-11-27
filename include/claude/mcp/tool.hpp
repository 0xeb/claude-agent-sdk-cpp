#ifndef CLAUDE_MCP_TOOL_HPP
#define CLAUDE_MCP_TOOL_HPP

#include <claude/mcp/argument_extractor.hpp>
#include <claude/mcp/type_traits.hpp>
#include <functional>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace claude
{
namespace mcp
{

// ============================================================================
// Tool Wrapper - Wraps a function as an MCP tool
// ============================================================================

/// Wraps a C++ function as an MCP tool with automatic schema generation
template <typename Func>
class ToolWrapper
{
  public:
    using Traits = FunctionTraits<remove_cvref_t<Func>>;
    using ReturnType = typename Traits::ReturnType;
    static constexpr size_t Arity = Traits::arity;

    /// Construct tool with name, description, and function
    ToolWrapper(std::string name, std::string description, Func&& func)
        : name_(std::move(name)), description_(std::move(description)),
          func_(std::forward<Func>(func)), param_names_(generate_param_names())
    {
        generate_schemas();
        // Initialize invoker after func_ is set using emplace
        std::array<std::string, Arity> names_array;
        for (size_t i = 0; i < Arity; ++i)
            names_array[i] = param_names_[i];
        invoker_.emplace(func_, names_array);
    }

    /// Get tool name
    const std::string& name() const
    {
        return name_;
    }

    /// Get tool description
    const std::string& description() const
    {
        return description_;
    }

    /// Get input schema (JSON Schema for parameters)
    const json& input_schema() const
    {
        return input_schema_;
    }

    /// Get output schema (JSON Schema for return type)
    const json& output_schema() const
    {
        return output_schema_;
    }

    /// Invoke tool with JSON arguments, return JSON result
    json invoke(const json& args) const
    {
        // JsonInvoker is not const, so we need to make a mutable copy
        auto mutable_invoker = *invoker_;
        return mutable_invoker(args);
    }

    /// Get parameter names
    const std::vector<std::string>& param_names() const
    {
        return param_names_;
    }

  private:
    /// Generate parameter names (arg0, arg1, arg2, ...)
    std::vector<std::string> generate_param_names() const
    {
        std::vector<std::string> names;
        for (size_t i = 0; i < Arity; ++i)
            names.push_back("arg" + std::to_string(i));
        return names;
    }

    /// Generate input and output schemas
    void generate_schemas()
    {
        // Generate input schema
        input_schema_ =
            json{{"type", "object"}, {"properties", json::object()}, {"required", json::array()}};

        // Add each parameter to the schema
        generate_param_schemas<0>();

        // Generate output schema
        output_schema_ = TypeToSchema<ReturnType>::get();
    }

    /// Generate parameter schemas (recursive template)
    template <size_t Index>
    void generate_param_schemas()
    {
        if constexpr (Index < Arity)
        {
            using ArgType = typename Traits::template ArgType<Index>;
            std::string param_name = param_names_[Index];

            // Add parameter schema
            input_schema_["properties"][param_name] = TypeToSchema<ArgType>::get();

            // Add to required list
            input_schema_["required"].push_back(param_name);

            // Recurse for next parameter
            generate_param_schemas<Index + 1>();
        }
    }

    std::string name_;
    std::string description_;
    Func func_;
    std::vector<std::string> param_names_;
    json input_schema_;
    json output_schema_;
    std::optional<JsonInvoker<Func>> invoker_;
};

// ============================================================================
// Tool Factory Functions
// ============================================================================

/// Create a tool from a function (automatic type deduction)
template <typename Func>
auto make_tool(std::string name, std::string description, Func&& func)
{
    return ToolWrapper<Func>(std::move(name), std::move(description), std::forward<Func>(func));
}

/// Create a tool with custom parameter names
template <typename Func>
class ToolWrapperWithNames
{
  public:
    using Traits = FunctionTraits<remove_cvref_t<Func>>;
    using ReturnType = typename Traits::ReturnType;
    static constexpr size_t Arity = Traits::arity;

    ToolWrapperWithNames(std::string name, std::string description, Func&& func,
                         std::vector<std::string> param_names)
        : name_(std::move(name)), description_(std::move(description)),
          func_(std::forward<Func>(func)), param_names_(std::move(param_names))
    {
        validate_param_count();
        generate_schemas();
        // Initialize invoker using emplace
        std::array<std::string, Arity> names_array;
        for (size_t i = 0; i < Arity; ++i)
            names_array[i] = param_names_[i];
        invoker_.emplace(func_, names_array);
    }

    const std::string& name() const
    {
        return name_;
    }
    const std::string& description() const
    {
        return description_;
    }
    const json& input_schema() const
    {
        return input_schema_;
    }
    const json& output_schema() const
    {
        return output_schema_;
    }
    const std::vector<std::string>& param_names() const
    {
        return param_names_;
    }

    json invoke(const json& args) const
    {
        auto mutable_invoker = *invoker_;
        return mutable_invoker(args);
    }

  private:
    void validate_param_count()
    {
        if (param_names_.size() != Arity)
        {
            throw std::invalid_argument("Parameter name count mismatch for tool '" + name_ +
                                        "': expected " + std::to_string(Arity) + ", got " +
                                        std::to_string(param_names_.size()));
        }
    }

    void generate_schemas()
    {
        input_schema_ =
            json{{"type", "object"}, {"properties", json::object()}, {"required", json::array()}};

        generate_param_schemas<0>();
        output_schema_ = TypeToSchema<ReturnType>::get();
    }

    template <size_t Index>
    void generate_param_schemas()
    {
        if constexpr (Index < Arity)
        {
            using ArgType = typename Traits::template ArgType<Index>;
            std::string param_name = param_names_[Index];

            input_schema_["properties"][param_name] = TypeToSchema<ArgType>::get();
            input_schema_["required"].push_back(param_name);

            generate_param_schemas<Index + 1>();
        }
    }

    std::string name_;
    std::string description_;
    Func func_;
    std::vector<std::string> param_names_;
    json input_schema_;
    json output_schema_;
    std::optional<JsonInvoker<Func>> invoker_;
};

/// Create a tool with custom parameter names
template <typename Func>
auto make_tool(std::string name, std::string description, Func&& func,
               std::vector<std::string> param_names)
{
    return ToolWrapperWithNames<Func>(std::move(name), std::move(description),
                                      std::forward<Func>(func), std::move(param_names));
}

// ============================================================================
// Type-erased Tool Interface (for storage in containers)
// ============================================================================

/// Type-erased tool interface
class Tool
{
  public:
    template <typename ToolWrapper>
    Tool(ToolWrapper&& wrapper)
        : name_(wrapper.name()), description_(wrapper.description()),
          input_schema_(wrapper.input_schema()), output_schema_(wrapper.output_schema()),
          invoker_([w = std::forward<ToolWrapper>(wrapper)](const json& args) mutable
                   { return w.invoke(args); })
    {
    }

    const std::string& name() const
    {
        return name_;
    }
    const std::string& description() const
    {
        return description_;
    }
    const json& input_schema() const
    {
        return input_schema_;
    }
    const json& output_schema() const
    {
        return output_schema_;
    }

    json invoke(const json& args) const
    {
        return invoker_(args);
    }

    // Expose a copy of the invoker for safer captures
    std::function<json(const json&)> invoker() const
    {
        return invoker_;
    }

  private:
    std::string name_;
    std::string description_;
    json input_schema_;
    json output_schema_;
    std::function<json(const json&)> invoker_;
};

} // namespace mcp
} // namespace claude

#endif // CLAUDE_MCP_TOOL_HPP
