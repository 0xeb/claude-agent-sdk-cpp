#ifndef CLAUDE_MCP_TYPE_TRAITS_HPP
#define CLAUDE_MCP_TYPE_TRAITS_HPP

#include <nlohmann/json.hpp>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>
#include <map>

namespace claude
{
namespace mcp
{

// Forward declarations
using json = nlohmann::json;

// ============================================================================
// Helper Utilities
// ============================================================================

/// Always-false helper for static_assert in if constexpr branches
template <typename T>
struct always_false : std::false_type
{
};

/// Helper to remove cv-ref qualifiers
template <typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

// ============================================================================
// Type to JSON Schema Mapping
// ============================================================================

/// Maps C++ types to JSON Schema objects
/// Supports: bool, int, long, float, double, string, json, vector, map
template <typename T>
struct TypeToSchema
{
    static json get()
    {
        using BaseType = remove_cvref_t<T>;

        // Void type (for functions with no return value)
        if constexpr (std::is_same_v<BaseType, void>)
        {
            return json{{"type", "null"}};
        }
        // Primitive types
        else if constexpr (std::is_same_v<BaseType, bool>)
        {
            return json{{"type", "boolean"}};
        }
        else if constexpr (std::is_same_v<BaseType, int> || std::is_same_v<BaseType, long> ||
                           std::is_same_v<BaseType, int32_t> || std::is_same_v<BaseType, int64_t> ||
                           std::is_same_v<BaseType, unsigned int> || std::is_same_v<BaseType, unsigned long> ||
                           std::is_same_v<BaseType, uint32_t> || std::is_same_v<BaseType, uint64_t>)
        {
            return json{{"type", "integer"}};
        }
        else if constexpr (std::is_same_v<BaseType, float> || std::is_same_v<BaseType, double>)
        {
            return json{{"type", "number"}};
        }
        else if constexpr (std::is_same_v<BaseType, std::string> || std::is_same_v<BaseType, const char*> ||
                           (std::is_array_v<BaseType> && std::is_same_v<std::remove_extent_t<BaseType>, char>))
        {
            return json{{"type", "string"}};
        }
        else if constexpr (std::is_same_v<BaseType, json>)
        {
            return json{{"type", "object"}};
        }
        // Array types
        else if constexpr (is_vector<BaseType>::value)
        {
            using ItemType = typename BaseType::value_type;
            return json{{"type", "array"}, {"items", TypeToSchema<ItemType>::get()}};
        }
        // Object types (map<string, T>)
        else if constexpr (is_string_map<BaseType>::value)
        {
            using ValueType = typename BaseType::mapped_type;
            json schema = {{"type", "object"}};
            // Could add additionalProperties schema for map values
            schema["additionalProperties"] = TypeToSchema<ValueType>::get();
            return schema;
        }
        else
        {
            static_assert(always_false<T>::value,
                          "Unsupported type for MCP tool. Supported types: "
                          "bool, int, long, float, double, std::string, json, "
                          "std::vector<T>, std::map<std::string, T>");
            return json{};
        }
    }

private:
    // Helper to detect std::vector<T>
    template <typename U>
    struct is_vector : std::false_type
    {
    };

    template <typename U, typename Alloc>
    struct is_vector<std::vector<U, Alloc>> : std::true_type
    {
    };

    // Helper to detect std::map<std::string, T>
    template <typename U>
    struct is_string_map : std::false_type
    {
    };

    template <typename V, typename Compare, typename Alloc>
    struct is_string_map<std::map<std::string, V, Compare, Alloc>> : std::true_type
    {
    };
};

// ============================================================================
// Function Signature Traits
// ============================================================================

/// Extract traits from function pointers
template <typename Func>
struct FunctionTraits;

// Function pointer: Ret(*)(Args...)
template <typename Ret, typename... Args>
struct FunctionTraits<Ret (*)(Args...)>
{
    using ReturnType = Ret;
    using ArgsTuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);

    template <size_t N>
    using ArgType = std::tuple_element_t<N, ArgsTuple>;
};

// Lambda and std::function: operator() const
template <typename Ret, typename Class, typename... Args>
struct FunctionTraits<Ret (Class::*)(Args...) const>
{
    using ReturnType = Ret;
    using ArgsTuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);

    template <size_t N>
    using ArgType = std::tuple_element_t<N, ArgsTuple>;
};

// Lambda and std::function: operator() non-const
template <typename Ret, typename Class, typename... Args>
struct FunctionTraits<Ret (Class::*)(Args...)>
{
    using ReturnType = Ret;
    using ArgsTuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);

    template <size_t N>
    using ArgType = std::tuple_element_t<N, ArgsTuple>;
};

// Regular function: Ret(Args...)
template <typename Ret, typename... Args>
struct FunctionTraits<Ret(Args...)>
{
    using ReturnType = Ret;
    using ArgsTuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);

    template <size_t N>
    using ArgType = std::tuple_element_t<N, ArgsTuple>;
};

// Auto-decay to operator() for lambdas and functors
template <typename Func>
struct FunctionTraits : FunctionTraits<decltype(&remove_cvref_t<Func>::operator())>
{
};

// ============================================================================
// Return Type Helpers
// ============================================================================

/// Check if return type is void
template <typename Func>
struct HasVoidReturn
{
    using Traits = FunctionTraits<remove_cvref_t<Func>>;
    static constexpr bool value = std::is_same_v<typename Traits::ReturnType, void>;
};

/// Helper variable template
template <typename Func>
inline constexpr bool has_void_return_v = HasVoidReturn<Func>::value;

// ============================================================================
// Argument Count Helpers
// ============================================================================

/// Get arity (argument count) for a function
template <typename Func>
struct Arity
{
    using Traits = FunctionTraits<remove_cvref_t<Func>>;
    static constexpr size_t value = Traits::arity;
};

/// Helper variable template
template <typename Func>
inline constexpr size_t arity_v = Arity<Func>::value;

} // namespace mcp
} // namespace claude

#endif // CLAUDE_MCP_TYPE_TRAITS_HPP
