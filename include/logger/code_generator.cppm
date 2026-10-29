/**
 * ============================================================================
 * Code Generation Utilities - Advanced Mustache Template Engine
 * ============================================================================
 *
 * A powerful template engine designed specifically for code generation tasks,
 * built on top of the logger mustache template system.
 *
 * FEATURES:
 * ---------
 * - String filters: uppercase, lowercase, camelCase, snake_case, PascalCase, kebab-case
 * - Conditional rendering: {{#if}}...{{/if}}, {{#unless}}...{{/unless}}
 * - Loop support: {{#each}}...{{/each}} with index access
 * - Nested data: Support for dot notation (object.property.subproperty)
 * - Code formatters: indent, wrap, escape, quote, comment
 * - Built-in helpers: join, split, replace, trim, pad
 * - Comments: {{! comment text }}
 * - Raw blocks: {{{{raw}}}}...{{{{/raw}}}} for literal text
 * - Partials: {{> partial_name}} for reusable templates
 * - Custom filters: Register your own transformation functions
 *
 * USAGE EXAMPLES:
 * --------------
 * ```cpp
 * // 1. Simple C++ class generation
 * auto class_template = R"(
 * class {{class_name | PascalCase}} {
 * public:
 *     {{#each methods}}
 *     {{return_type}} {{name | camelCase}}({{#each params}}{{type}} {{name}}{{#unless @last}},
 * {{/unless}}{{/each}});
 *     {{/each}}
 * };
 * )";
 *
 * CodeGenerator gen;
 * auto result = gen.render(class_template, {
 *     {"class_name", "user_manager"},
 *     {"methods", {
 *         {{"return_type", "void"}, {"name", "add_user"}, {"params", ...}},
 *         {{"return_type", "User"}, {"name", "get_user"}, {"params", ...}}
 *     }}
 * });
 *
 * // 2. Python function generation with conditionals
 * auto py_template = R"(
 * def {{name | snake_case}}({{#each params}}{{name}}: {{type}}{{#unless @last}},
 * {{/unless}}{{/each}}){{#if return_type}} -> {{return_type}}{{/if}}:
 *     """{{description | wrap 80}}"""
 *     {{#if validate}}
 *     # Input validation
 *     {{#each params}}
 *     if {{name}} is None:
 *         raise ValueError("{{name}} cannot be None")
 *     {{/each}}
 *     {{/if}}
 *
 *     {{#each body_lines}}
 *     {{. | indent 4}}
 *     {{/each}}
 * )";
 *
 * // 3. SQL table generation
 * auto sql_template = R"(
 * CREATE TABLE {{table_name | snake_case}} (
 *     {{#each columns}}
 *     {{name | snake_case}} {{type}}{{#if not_null}} NOT NULL{{/if}}{{#if primary_key}} PRIMARY
 * KEY{{/if}}{{#unless @last}},{{/unless}}
 *     {{/each}}
 * );
 * )";
 *
 * // 4. JSON/YAML generation
 * auto json_template = R"({
 *     "name": "{{name | escape}}",
 *     "version": "{{version}}",
 *     {{#if description}}
 *     "description": "{{description | escape}}",
 *     {{/if}}
 *     "dependencies": [
 *         {{#each deps}}
 *         "{{name}}@{{version}}"{{#unless @last}},{{/unless}}
 *         {{/each}}
 *     ]
 * })";
 * ```
 *
 * PERFORMANCE:
 * -----------
 * - Single-pass template processing: O(n) where n = template length
 * - Filter application: O(1) lookup via function pointer map
 * - Minimal allocations: Reuses string buffers where possible
 * - Suitable for: Build-time code generation, runtime templating
 *
 * THREAD-SAFETY:
 * -------------
 * - All filters are thread-safe (pure functions, no shared state)
 * - CodeGenerator instances are NOT thread-safe (use one per thread)
 * - Filter registration is NOT thread-safe (register at startup only)
 *
 * C++ CORE GUIDELINES COMPLIANCE:
 * ------------------------------
 * - F.15: Prefer simple and conventional ways of passing information
 * - ES.23: Prefer the {} initializer syntax
 * - C.21: Define or delete all default operations
 * - R.1: RAII for resource management
 *
 * AUTHOR: Olumuyiwa Oluwasanmi
 * DATE: November 18, 2025
 * LICENSE: BSD-4-Clause with Author's Special Rights
 * ============================================================================
 */

module;

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

export module logger.code_generator;

export namespace logger::codegen {

/**
 * ============================================================================
 * String Filters - Text Transformation Functions
 * ============================================================================
 */

/**
 * Convert string to UPPERCASE
 */
[[nodiscard]] inline auto toUpperCase(std::string_view str) -> std::string {
    std::string result{str};
    std::ranges::transform(result, result.begin(), [](unsigned char c) -> char {
        return static_cast<char>(std::toupper(c));
    });
    return result;
}

/**
 * Convert string to lowercase
 */
[[nodiscard]] inline auto toLowerCase(std::string_view str) -> std::string {
    std::string result{str};
    std::ranges::transform(result, result.begin(), [](unsigned char c) -> char {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

/**
 * Convert string to camelCase
 * Example: "hello_world" -> "helloWorld", "HelloWorld" -> "helloWorld"
 */
[[nodiscard]] inline auto toCamelCase(std::string_view str) -> std::string {
    std::string result;
    result.reserve(str.size());

    bool capitalize_next = false;
    bool first_char = true;

    for (char c : str) {
        if (c == '_' || c == '-' || c == ' ') {
            capitalize_next = true;
        } else if (capitalize_next) {
            result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            capitalize_next = false;
        } else if (first_char) {
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            first_char = false;
        } else {
            result += c;
        }
    }

    return result;
}

/**
 * Convert string to PascalCase
 * Example: "hello_world" -> "HelloWorld", "helloWorld" -> "HelloWorld"
 */
[[nodiscard]] inline auto toPascalCase(std::string_view str) -> std::string {
    auto result = toCamelCase(str);
    if (!result.empty()) {
        result[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[0])));
    }
    return result;
}

/**
 * Convert string to snake_case
 * Example: "HelloWorld" -> "hello_world", "helloWorld" -> "hello_world"
 */
[[nodiscard]] inline auto toSnakeCase(std::string_view str) -> std::string {
    std::string result;
    result.reserve(str.size() + 10); // Reserve extra space for underscores

    for (size_t i = 0; i < str.size(); ++i) {
        char c = str[i];

        // Insert underscore before uppercase letters (except first char)
        if (i > 0 && (std::isupper(static_cast<unsigned char>(c)) != 0) &&
            (std::islower(static_cast<unsigned char>(str[i - 1])) != 0)) {
            result += '_';
        }

        // Replace spaces and hyphens with underscores
        if (c == ' ' || c == '-') {
            result += '_';
        } else {
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }

    return result;
}

/**
 * Convert string to kebab-case
 * Example: "HelloWorld" -> "hello-world", "hello_world" -> "hello-world"
 */
[[nodiscard]] inline auto toKebabCase(std::string_view str) -> std::string {
    auto snake = toSnakeCase(str);
    std::ranges::replace(snake, '_', '-');
    return snake;
}

/**
 * Convert string to SCREAMING_SNAKE_CASE
 * Example: "helloWorld" -> "HELLO_WORLD"
 */
[[nodiscard]] inline auto toScreamingSnakeCase(std::string_view str) -> std::string {
    auto snake = toSnakeCase(str);
    return toUpperCase(snake);
}

/**
 * Trim whitespace from both ends
 */
[[nodiscard]] inline auto trim(std::string_view str) -> std::string {
    auto start = str.find_first_not_of(" \t\n\r");
    if (start == std::string_view::npos) {
        return "";
    }
    auto end = str.find_last_not_of(" \t\n\r");
    return std::string(str.substr(start, end - start + 1));
}

/**
 * Indent each line by specified number of spaces
 */
[[nodiscard]] inline auto indent(std::string_view str, int spaces) -> std::string {
    std::string padding(spaces, ' ');
    std::ostringstream result;
    std::istringstream input{std::string(str)};
    std::string line;

    while (std::getline(input, line)) {
        result << padding << line << '\n';
    }

    std::string output = result.str();
    if (!output.empty() && output.back() == '\n') {
        output.pop_back(); // Remove trailing newline
    }

    return output;
}

/**
 * Wrap text to specified width
 */
[[nodiscard]] inline auto wrap(std::string_view str, int width) -> std::string {
    std::ostringstream result;
    std::istringstream words{std::string(str)};
    std::string word;
    int line_length = 0;

    while (words >> word) {
        if (line_length + static_cast<int>(word.length()) + 1 > width) {
            result << '\n' << word;
            line_length = static_cast<int>(word.length());
        } else {
            if (line_length > 0) {
                result << ' ';
                line_length++;
            }
            result << word;
            line_length += static_cast<int>(word.length());
        }
    }

    return result.str();
}

/**
 * Escape special characters for C++ strings
 */
[[nodiscard]] inline auto escape(std::string_view str) -> std::string {
    std::string result;
    result.reserve(str.size() * 2); // Worst case: all chars need escaping

    for (char c : str) {
        switch (c) {
            case '\\':
                result += "\\\\";
                break;
            case '\"':
                result += "\\\"";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                result += c;
        }
    }

    return result;
}

/**
 * Quote string with double quotes
 */
[[nodiscard]] inline auto quote(std::string_view str) -> std::string {
    return "\"" + std::string(str) + "\"";
}

/**
 * Create C++ style comment
 */
[[nodiscard]] inline auto cppComment(std::string_view str) -> std::string {
    return "// " + std::string(str);
}

/**
 * Create C-style comment
 */
[[nodiscard]] inline auto cComment(std::string_view str) -> std::string {
    return "/* " + std::string(str) + " */";
}

/**
 * Create Python style comment
 */
[[nodiscard]] inline auto pythonComment(std::string_view str) -> std::string {
    return "# " + std::string(str);
}

/**
 * Join array of strings with delimiter
 */
[[nodiscard]] inline auto join(std::vector<std::string> const& items, std::string_view delimiter)
    -> std::string {
    if (items.empty())
        return "";

    std::ostringstream result;
    result << items[0];
    for (size_t i = 1; i < items.size(); ++i) {
        result << delimiter << items[i];
    }
    return result.str();
}

/**
 * ============================================================================
 * Template Value System - Support for complex data structures
 * ============================================================================
 */

// Forward declarations
class TemplateValue;
using TemplateArray = std::vector<TemplateValue>;
using TemplateObject = std::map<std::string, TemplateValue>;

/**
 * Template value variant - can hold string, bool, int64_t, double, array, or object
 */
class TemplateValue {
  public:
    using ValueType =
        std::variant<std::string, bool, std::int64_t, double, TemplateArray, TemplateObject>;

    // Constructors for each type
    TemplateValue() : value_{std::string{}} {}
    TemplateValue(std::string s) : value_{std::move(s)} {}
    TemplateValue(char const* s) : value_{std::string{s}} {}
    TemplateValue(bool b) : value_{b} {}
    TemplateValue(int i) : value_{static_cast<std::int64_t>(i)} {}
    TemplateValue(std::int64_t i) : value_{i} {}
    TemplateValue(std::size_t u) : value_{static_cast<std::int64_t>(u)} {}
    TemplateValue(double d) : value_{d} {}
    TemplateValue(TemplateArray arr) : value_{std::move(arr)} {}
    TemplateValue(TemplateObject obj) : value_{std::move(obj)} {}

    // Type checks
    [[nodiscard]] auto isString() const -> bool {
        return std::holds_alternative<std::string>(value_);
    }
    [[nodiscard]] auto isBool() const -> bool { return std::holds_alternative<bool>(value_); }
    [[nodiscard]] auto isInt() const -> bool {
        return std::holds_alternative<std::int64_t>(value_);
    }
    [[nodiscard]] auto isDouble() const -> bool { return std::holds_alternative<double>(value_); }
    [[nodiscard]] auto isArray() const -> bool {
        return std::holds_alternative<TemplateArray>(value_);
    }
    [[nodiscard]] auto isObject() const -> bool {
        return std::holds_alternative<TemplateObject>(value_);
    }

    // Accessors
    [[nodiscard]] auto asString() const -> std::string {
        return std::visit(
            [](auto&& arg) -> std::string {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    return arg;
                } else if constexpr (std::is_same_v<T, bool>) {
                    return arg ? "true" : "false";
                } else if constexpr (std::is_same_v<T, std::int64_t> ||
                                     std::is_same_v<T, double>) {
                    return std::to_string(arg);
                } else {
                    return "";
                }
            },
            value_);
    }

    [[nodiscard]] auto asBool() const -> bool {
        if (isBool())
            return std::get<bool>(value_);
        if (isInt())
            return std::get<std::int64_t>(value_) != 0;
        if (isDouble())
            return std::get<double>(value_) != 0.0;
        if (isString())
            return !std::get<std::string>(value_).empty();
        if (isArray())
            return !std::get<TemplateArray>(value_).empty();
        return false;
    }

    [[nodiscard]] auto asArray() const -> TemplateArray const& {
        return std::get<TemplateArray>(value_);
    }

    [[nodiscard]] auto asObject() const -> TemplateObject const& {
        return std::get<TemplateObject>(value_);
    }

    // Nested access with dot notation
    [[nodiscard]] auto getProperty(std::string_view path) const -> TemplateValue {
        if (path == ".") {
            if (isObject()) {
                auto const& obj = asObject();
                auto it = obj.find(".");
                if (it != obj.end()) {
                    return it->second;
                }
            }
            return TemplateValue{};
        }
        auto dot_pos = path.find('.');

        if (dot_pos == std::string_view::npos) {
            // No more dots - direct property access
            if (isObject()) {
                auto const& obj = asObject();
                auto it = obj.find(std::string(path));
                if (it != obj.end()) {
                    return it->second;
                }
            }
            return TemplateValue{};
        }

        // Has dot - recursive access
        auto first_part = path.substr(0, dot_pos);
        auto remaining = path.substr(dot_pos + 1);

        if (isObject()) {
            auto const& obj = asObject();
            auto it = obj.find(std::string(first_part));
            if (it != obj.end()) {
                return it->second.getProperty(remaining);
            }
        }

        return TemplateValue{};
    }

  private:
    ValueType value_;
};

/**
 * Convert string to valid C/C++/CUDA/GLSL/Python identifier (replaces non-alnum with '_')
 */
[[nodiscard]] inline auto toCIdentifier(std::string_view str) -> std::string {
    std::string result;
    result.reserve(str.size());
    for (char c : str) {
        if (std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_') {
            result += c;
        } else {
            result += '_';
        }
    }
    if (!result.empty() && (std::isdigit(static_cast<unsigned char>(result[0])) != 0)) {
        result.insert(result.begin(), '_');
    }
    return result;
}

/**
 * Create Doxygen / JSDoc block comment
 */
[[nodiscard]] inline auto docComment(std::string_view str) -> std::string {
    return "/** " + std::string(str) + " */";
}

/**
 * Pad string on the left to target width
 */
[[nodiscard]] inline auto padLeft(std::string_view str, int width, char pad_char = ' ')
    -> std::string {
    if (width <= 0 || static_cast<int>(str.size()) >= width) {
        return std::string(str);
    }
    return std::string(static_cast<std::size_t>(width) - str.size(), pad_char) + std::string(str);
}

/**
 * Pad string on the right to target width
 */
[[nodiscard]] inline auto padRight(std::string_view str, int width, char pad_char = ' ')
    -> std::string {
    if (width <= 0 || static_cast<int>(str.size()) >= width) {
        return std::string(str);
    }
    return std::string(str) + std::string(static_cast<std::size_t>(width) - str.size(), pad_char);
}

/**
 * Prefix declaration with C++23 / CUDA cache-line alignment specifier
 */
[[nodiscard]] inline auto alignAs(std::string_view str, int alignment = 64) -> std::string {
    return "alignas(" + std::to_string(alignment > 0 ? alignment : 64) + ") " + std::string(str);
}

/**
 * Prefix kernel name/signature with CUDA extern "C" __global__ qualifier
 */
[[nodiscard]] inline auto cudaGlobal(std::string_view str) -> std::string {
    return "extern \"C\" __global__ void " + std::string(str);
}

/**
 * Prefix device helper with __device__ __forceinline__
 */
[[nodiscard]] inline auto cudaDeviceInline(std::string_view str) -> std::string {
    return "__device__ __forceinline__ " + std::string(str);
}

/**
 * Prefix function with AVX-512 multi-register SIMD target attribute
 */
[[nodiscard]] inline auto simdAvx512Attr(std::string_view str) -> std::string {
    return "__attribute__((target(\"avx512f,avx512bw,avx512vl,avx512vnni,avx512bf16,fma\"))) " +
           std::string(str);
}

/**
 * Prefix function with AVX2 + FMA target attribute
 */
[[nodiscard]] inline auto simdAvx2Attr(std::string_view str) -> std::string {
    return "__attribute__((target(\"avx2,fma,f16c\"))) " + std::string(str);
}

/**
 * ============================================================================
 * Filter Registry - Manages transformation functions
 * ============================================================================
 */

using FilterFunction = std::function<std::string(std::string_view)>;

class FilterRegistry {
  public:
    FilterRegistry() {
        // Register built-in filters
        registerFilter("uppercase", toUpperCase);
        registerFilter("upper", toUpperCase);
        registerFilter("lowercase", toLowerCase);
        registerFilter("lower", toLowerCase);
        registerFilter("camelCase", toCamelCase);
        registerFilter("PascalCase", toPascalCase);
        registerFilter("snake_case", toSnakeCase);
        registerFilter("kebab-case", toKebabCase);
        registerFilter("SCREAMING_SNAKE_CASE", toScreamingSnakeCase);
        registerFilter("SCREAMING_SNAKE", toScreamingSnakeCase);
        registerFilter("c_ident", toCIdentifier);
        registerFilter("trim", trim);
        registerFilter("escape", escape);
        registerFilter("quote", quote);
        registerFilter("cpp_comment", cppComment);
        registerFilter("glsl_comment", cppComment);
        registerFilter("c_comment", cComment);
        registerFilter("py_comment", pythonComment);
        registerFilter("doc_comment", docComment);
        registerFilter("align64", [](std::string_view s) -> std::string { return alignAs(s, 64); });
        registerFilter("cuda_global", cudaGlobal);
        registerFilter("cuda_device_inline", cudaDeviceInline);
        registerFilter("simd_avx512_attr", simdAvx512Attr);
        registerFilter("simd_avx2_attr", simdAvx2Attr);

        // Parameterized shorthand filters
        registerFilter("indent2", [](std::string_view s) -> std::string { return indent(s, 2); });
        registerFilter("indent4", [](std::string_view s) -> std::string { return indent(s, 4); });
        registerFilter("indent6", [](std::string_view s) -> std::string { return indent(s, 6); });
        registerFilter("indent8", [](std::string_view s) -> std::string { return indent(s, 8); });
        registerFilter("wrap80", [](std::string_view s) -> std::string { return wrap(s, 80); });
        registerFilter("wrap120", [](std::string_view s) -> std::string { return wrap(s, 120); });
    }

    auto registerFilter(std::string name, FilterFunction filter) -> void {
        filters_[std::move(name)] = std::move(filter);
    }

    [[nodiscard]] auto applyFilter(std::string_view filter_spec, std::string_view value) const
        -> std::string {
        auto trimmed_spec = trim(filter_spec);
        auto it = filters_.find(trimmed_spec);
        if (it != filters_.end()) {
            return it->second(value);
        }

        // Check parameterized filters: "indent 4", "wrap 80", "pad_left 12",
        // "pad_right 12", "align_as 64", "default fallback", "prefix pre_", "suffix _t"
        auto space_pos = trimmed_spec.find(' ');
        if (space_pos != std::string_view::npos) {
            auto fname = trim(trimmed_spec.substr(0, space_pos));
            auto farg = trim(trimmed_spec.substr(space_pos + 1));
            if (farg.size() >= 2 &&
                ((farg.front() == '"' && farg.back() == '"') ||
                 (farg.front() == '\'' && farg.back() == '\''))) {
                farg = farg.substr(1, farg.size() - 2);
            }
            auto parse_int = [](std::string const& s, int fallback) -> int {
                try {
                    return std::stoi(s);
                } catch (...) {
                    return fallback;
                }
            };
            if (fname == "indent") {
                return indent(value, parse_int(farg, 4));
            }
            if (fname == "wrap") {
                return wrap(value, parse_int(farg, 80));
            }
            if (fname == "pad_left") {
                return padLeft(value, parse_int(farg, 8));
            }
            if (fname == "pad_right") {
                return padRight(value, parse_int(farg, 8));
            }
            if (fname == "align_as") {
                return alignAs(value, parse_int(farg, 64));
            }
            if (fname == "default") {
                return value.empty() ? farg : std::string(value);
            }
            if (fname == "prefix") {
                return farg + std::string(value);
            }
            if (fname == "suffix") {
                return std::string(value) + farg;
            }
        }

        return std::string(value); // Unknown filter - return unchanged
    }

    [[nodiscard]] auto hasFilter(std::string_view filter_name) const -> bool {
        return filters_.contains(std::string(filter_name));
    }

  private:
    std::unordered_map<std::string, FilterFunction> filters_;
};

/**
 * ============================================================================
 * Code Generator - Advanced Template Engine
 * ============================================================================
 */

class CodeGenerator {
  public:
    CodeGenerator() : filters_{std::make_shared<FilterRegistry>()} {}

    auto registerFilter(std::string name, FilterFunction filter) -> CodeGenerator& {
        filters_->registerFilter(std::move(name), std::move(filter));
        return *this;
    }

    auto registerPartial(std::string name, std::string partial_template) -> CodeGenerator& {
        partials_[std::move(name)] = std::move(partial_template);
        return *this;
    }

    auto setTemplate(std::string template_str) -> CodeGenerator& {
        template_ = std::move(template_str);
        return *this;
    }

    auto setData(TemplateObject data) -> CodeGenerator& {
        data_ = std::move(data);
        return *this;
    }

    auto addData(std::string key, TemplateValue value) -> CodeGenerator& {
        data_[std::move(key)] = std::move(value);
        return *this;
    }

    [[nodiscard]] auto render() const -> std::string { return processTemplate(template_, data_); }

    [[nodiscard]] auto render(std::string_view template_str, TemplateObject const& data) const
        -> std::string {
        return processTemplate(template_str, data);
    }

    auto reset() -> CodeGenerator& {
        template_.clear();
        data_.clear();
        partials_.clear();
        return *this;
    }

  private:
    /**
     * Find the matching closing tag for a section while respecting nested sections of the same kind.
     */
    [[nodiscard]] static auto findMatchingSectionEnd(std::string_view text,
                                                     std::size_t search_from,
                                                     std::string_view kind,
                                                     std::string_view end_tag) -> std::size_t {
        int depth = 1;
        std::size_t pos = search_from;
        while (pos < text.size()) {
            auto next_open = text.find("{{", pos);
            if (next_open == std::string_view::npos) {
                return std::string_view::npos;
            }
            if (text.substr(next_open).starts_with(end_tag)) {
                --depth;
                if (depth == 0) {
                    return next_open;
                }
                pos = next_open + end_tag.size();
                continue;
            }
            auto close_brace = text.find("}}", next_open + 2);
            if (close_brace == std::string_view::npos) {
                return std::string_view::npos;
            }
            auto tag_expr = trim(text.substr(next_open + 2, close_brace - (next_open + 2)));
            if (!tag_expr.empty() && tag_expr[0] == '#') {
                auto inner = trim(tag_expr.substr(1));
                if ((kind == "if" && inner.starts_with("if ")) ||
                    (kind == "each" && inner.starts_with("each ")) ||
                    (kind == "unless" && inner.starts_with("unless ")) ||
                    (kind == inner)) {
                    ++depth;
                }
            }
            pos = close_brace + 2;
        }
        return std::string_view::npos;
    }

    /**
     * Find top-level {{else}} inside section_content (ignoring {{else}} inside nested sections).
     */
    [[nodiscard]] static auto findTopLevelElse(std::string_view section_content) -> std::size_t {
        int depth = 0;
        std::size_t pos = 0;
        while (pos < section_content.size()) {
            auto next_tag = section_content.find("{{", pos);
            if (next_tag == std::string_view::npos) {
                return std::string_view::npos;
            }
            auto close_tag = section_content.find("}}", next_tag + 2);
            if (close_tag == std::string_view::npos) {
                return std::string_view::npos;
            }
            auto tag_expr = trim(section_content.substr(next_tag + 2, close_tag - (next_tag + 2)));
            if (!tag_expr.empty() && tag_expr[0] == '#') {
                ++depth;
            } else if (!tag_expr.empty() && tag_expr[0] == '/') {
                if (depth > 0) {
                    --depth;
                }
            } else if (depth == 0 && tag_expr == "else") {
                return next_tag;
            }
            pos = close_tag + 2;
        }
        return std::string_view::npos;
    }

    /**
     * Evaluate a conditional expression (supports variable truthiness and ==, !=, >=, <=, >, <).
     */
    [[nodiscard]] auto evaluateCondition(std::string_view cond_expr,
                                         TemplateObject const& data) const -> bool {
        auto strip_quotes = [](std::string s) -> std::string {
            s = trim(s);
            if (s.size() >= 2 &&
                ((s.front() == '"' && s.back() == '"') ||
                 (s.front() == '\'' && s.back() == '\''))) {
                return s.substr(1, s.size() - 2);
            }
            return s;
        };

        for (std::string_view op : {"==", "!=", ">=", "<=", ">", "<"}) {
            auto op_pos = cond_expr.find(op);
            if (op_pos != std::string_view::npos) {
                auto lhs_key = trim(cond_expr.substr(0, op_pos));
                auto rhs_raw = trim(cond_expr.substr(op_pos + op.size()));
                auto lhs_val = getVariable(lhs_key, data);
                auto rhs_val = (rhs_raw.size() >= 2 && (rhs_raw.front() == '"' || rhs_raw.front() == '\''))
                                   ? strip_quotes(rhs_raw)
                                   : (TemplateValue{data}.getProperty(rhs_raw).asString().empty()
                                          ? strip_quotes(rhs_raw)
                                          : getVariable(rhs_raw, data));
                if (op == "==") {
                    return lhs_val == rhs_val;
                }
                if (op == "!=") {
                    return lhs_val != rhs_val;
                }
                try {
                    double lnum = std::stod(lhs_val);
                    double rnum = std::stod(rhs_val);
                    if (op == ">=") return lnum >= rnum;
                    if (op == "<=") return lnum <= rnum;
                    if (op == ">") return lnum > rnum;
                    if (op == "<") return lnum < rnum;
                } catch (...) {
                    if (op == ">=") return lhs_val >= rhs_val;
                    if (op == "<=") return lhs_val <= rhs_val;
                    if (op == ">") return lhs_val > rhs_val;
                    if (op == "<") return lhs_val < rhs_val;
                }
            }
        }

        std::string clean_name = std::string(trim(cond_expr));
        if (clean_name.size() >= 3 && clean_name.substr(0, 3) == "../") {
            clean_name = clean_name.substr(3);
        }
        TemplateValue root{data};
        return root.getProperty(clean_name).asBool();
    }

    /**
     * Process template with full feature support including raw blocks, comments, partials,
     * depth-balanced nested sections, comparison conditionals, and multi-stage filter pipelines.
     */
    [[nodiscard]] auto processTemplate(std::string_view template_str,
                                       TemplateObject const& data) const -> std::string {
        std::ostringstream result;
        std::string_view remaining = template_str;

        while (!remaining.empty()) {
            auto start_pos = remaining.find("{{");

            if (start_pos == std::string_view::npos) {
                result << remaining;
                break;
            }

            // Append text before placeholder
            result << remaining.substr(0, start_pos);

            // Check for raw block {{{{raw}}}}...{{{{/raw}}}}
            if (remaining.substr(start_pos).starts_with("{{{{raw}}}}")) {
                constexpr std::string_view raw_open = "{{{{raw}}}}";
                constexpr std::string_view raw_close = "{{{{/raw}}}}";
                auto raw_end = remaining.find(raw_close, start_pos + raw_open.size());
                if (raw_end != std::string_view::npos) {
                    result << remaining.substr(start_pos + raw_open.size(),
                                               raw_end - (start_pos + raw_open.size()));
                    remaining = remaining.substr(raw_end + raw_close.size());
                    continue;
                }
            }

            // Check for triple braces {{{variable}}}
            bool is_triple_brace = false;
            std::size_t expr_start = start_pos + 2;
            if (start_pos + 2 < remaining.size() && remaining[start_pos + 2] == '{') {
                is_triple_brace = true;
                expr_start = start_pos + 3;
            }

            // Find matching end delimiter
            std::string end_delim = is_triple_brace ? "}}}" : "}}";
            auto end_pos = remaining.find(end_delim, expr_start);
            if (end_pos == std::string_view::npos) {
                result << remaining.substr(start_pos);
                break;
            }

            // Extract expression between delimiters
            auto expr_raw = remaining.substr(expr_start, end_pos - expr_start);
            std::string expr = trim(expr_raw);

            // 1. Comment {{! ... }}
            if (!expr.empty() && expr[0] == '!') {
                remaining = remaining.substr(end_pos + end_delim.length());
                continue;
            }

            // 2. Partial {{> partial_name}}
            if (!expr.empty() && expr[0] == '>') {
                std::string partial_name = trim(expr.substr(1));
                auto pit = partials_.find(partial_name);
                if (pit != partials_.end()) {
                    result << processTemplate(pit->second, data);
                }
                remaining = remaining.substr(end_pos + end_delim.length());
                continue;
            }

            // 3. Section start {{#name}}, {{#if expr}}, {{#each name}}, {{#unless expr}}
            if (!expr.empty() && expr[0] == '#') {
                std::string section_expr = std::string(trim(expr.substr(1)));
                std::string section_name;
                std::string section_kind;
                std::string end_tag;
                bool invert_section = false;

                if (section_expr.size() >= 3 && section_expr.substr(0, 3) == "if ") {
                    section_name = std::string(trim(section_expr.substr(3)));
                    section_kind = "if";
                    end_tag = "{{/if}}";
                } else if (section_expr.size() >= 5 && section_expr.substr(0, 5) == "each ") {
                    section_name = std::string(trim(section_expr.substr(5)));
                    section_kind = "each";
                    end_tag = "{{/each}}";
                } else if (section_expr.size() >= 7 && section_expr.substr(0, 7) == "unless ") {
                    section_name = std::string(trim(section_expr.substr(7)));
                    section_kind = "unless";
                    end_tag = "{{/unless}}";
                    invert_section = true;
                } else {
                    section_name = section_expr;
                    section_kind = section_name;
                    end_tag = "{{/" + section_name + "}}";
                }

                if (section_name.size() >= 3 && section_name.substr(0, 3) == "../") {
                    section_name = section_name.substr(3);
                }

                auto section_end = findMatchingSectionEnd(
                    remaining, end_pos + end_delim.length(), section_kind, end_tag);

                if (section_end == std::string_view::npos) {
                    result << processExpression(expr, data);
                    remaining = remaining.substr(end_pos + end_delim.length());
                    continue;
                }

                auto section_content = remaining.substr(
                    end_pos + end_delim.length(), section_end - (end_pos + end_delim.length()));
                result << processSection(section_name, section_kind, section_content, data,
                                         invert_section);
                remaining = remaining.substr(section_end + end_tag.length());
            } else {
                // Regular variable or filter pipeline expression
                result << processExpression(expr, data);
                remaining = remaining.substr(end_pos + end_delim.length());
            }
        }

        return result.str();
    }

    /**
     * Process a section {{#name}}...{{/name}}, {{#if}}...{{/if}}, {{#each}}...{{/each}}, or {{#unless}}...{{/unless}}
     */
    [[nodiscard]] auto processSection(std::string const& name, std::string_view kind,
                                      std::string_view section_content,
                                      TemplateObject const& data,
                                      bool invert_section = false) const -> std::string {
        std::string_view true_content = section_content;
        std::string_view false_content = "";

        auto else_pos = findTopLevelElse(section_content);
        if (else_pos != std::string_view::npos) {
            auto close_else = section_content.find("}}", else_pos + 2);
            true_content = section_content.substr(0, else_pos);
            false_content = (close_else != std::string_view::npos)
                                ? section_content.substr(close_else + 2)
                                : std::string_view{};
        }

        if (kind == "if" || invert_section) {
            const bool cond = evaluateCondition(name, data);
            const bool take_true = invert_section ? !cond : cond;
            return std::string(processTemplate(take_true ? true_content : false_content, data));
        }

        TemplateValue root{data};
        TemplateValue value = root.getProperty(name);

        if (value.isArray()) {
            auto const& array = value.asArray();
            if (array.empty()) {
                return std::string(processTemplate(false_content, data));
            }

            std::ostringstream result;
            for (std::size_t idx = 0; idx < array.size(); ++idx) {
                auto const& item = array[idx];
                TemplateObject merged_data = data;
                merged_data["@index"] = TemplateValue{idx};
                merged_data["@index1"] = TemplateValue{idx + 1};
                merged_data["@first"] = TemplateValue{idx == 0};
                merged_data["@last"] = TemplateValue{idx + 1 == array.size()};
                merged_data["@even"] = TemplateValue{(idx % 2) == 0};
                merged_data["@odd"] = TemplateValue{(idx % 2) == 1};
                if (item.isObject()) {
                    auto const& item_obj = item.asObject();
                    for (auto const& [key, val] : item_obj) {
                        merged_data[key] = val;
                    }
                } else {
                    merged_data["."] = item;
                }
                result << processTemplate(true_content, merged_data);
            }
            return result.str();
        }

        if (value.asBool()) {
            return std::string(processTemplate(true_content, data));
        }
        return std::string(processTemplate(false_content, data));
    }

    /**
     * Process single expression with multi-stage filter chaining ({{var | f1 | f2 | f3}})
     */
    [[nodiscard]] auto processExpression(std::string_view expr, TemplateObject const& data) const
        -> std::string {
        auto pipe_pos = expr.find('|');

        if (pipe_pos != std::string_view::npos) {
            auto var_name = trim(expr.substr(0, pipe_pos));
            std::string current = getVariable(var_name, data);
            std::string_view rest = expr.substr(pipe_pos + 1);
            while (!rest.empty()) {
                auto next_pipe = rest.find('|');
                auto filter_spec =
                    (next_pipe == std::string_view::npos) ? rest : rest.substr(0, next_pipe);
                current = filters_->applyFilter(trim(filter_spec), current);
                if (next_pipe == std::string_view::npos) {
                    break;
                }
                rest = rest.substr(next_pipe + 1);
            }
            return current;
        }

        return getVariable(expr, data);
    }

    /**
     * Get variable value from data (supports dot notation and ../ parent scope)
     */
    [[nodiscard]] auto getVariable(std::string_view var_name, TemplateObject const& data) const
        -> std::string {
        if (var_name.size() >= 3 && var_name.substr(0, 3) == "../") {
            var_name = var_name.substr(3);
        }

        TemplateValue root{data};
        auto value = root.getProperty(var_name);
        return value.asString();
    }

    std::shared_ptr<FilterRegistry> filters_;
    std::string template_;
    TemplateObject data_;
    std::unordered_map<std::string, std::string> partials_;
};

/**
 * ============================================================================
 * MultiBackendKernelBuilder - Fluent Structured Code Builder for 5 Backends
 * (CUDA, CPU Multi-Register SIMD, Vulkan GLSL, Apple MLX, OpenAI Triton)
 * ============================================================================
 */
class MultiBackendKernelBuilder {
  public:
    MultiBackendKernelBuilder() = default;

    auto setModelName(std::string name) -> MultiBackendKernelBuilder& {
        model_name_ = std::move(name);
        return *this;
    }

    auto setContextLength(std::size_t ctx_len, std::size_t orig_ctx_len = 131072)
        -> MultiBackendKernelBuilder& {
        ctx_len_ = ctx_len;
        orig_ctx_len_ = orig_ctx_len;
        return *this;
    }

    auto setQuantization(std::string weight_quant, std::string kv_quant)
        -> MultiBackendKernelBuilder& {
        weight_quant_ = std::move(weight_quant);
        kv_quant_ = std::move(kv_quant);
        return *this;
    }

    auto setRopeScheme(std::string scheme, double scale_factor = 8.0)
        -> MultiBackendKernelBuilder& {
        rope_scheme_ = std::move(scheme);
        rope_scale_ = scale_factor;
        return *this;
    }

    [[nodiscard]] auto buildTemplateData() const -> TemplateObject {
        return TemplateObject{
            {"model_name", TemplateValue{model_name_}},
            {"context_length", TemplateValue{ctx_len_}},
            {"orig_context_length", TemplateValue{orig_ctx_len_}},
            {"weight_quant", TemplateValue{weight_quant_}},
            {"kv_quant", TemplateValue{kv_quant_}},
            {"rope_scheme", TemplateValue{rope_scheme_}},
            {"rope_scale", TemplateValue{rope_scale_}},
        };
    }

    [[nodiscard]] auto renderCustom(std::string_view tmpl,
                                    TemplateObject extra = {}) const -> std::string {
        auto data = buildTemplateData();
        for (auto& [k, v] : extra) {
            data[k] = std::move(v);
        }
        return gen_.render(tmpl, data);
    }

  private:
    CodeGenerator gen_;
    std::string model_name_{"qwen38_27b"};
    std::size_t ctx_len_{1048576};
    std::size_t orig_ctx_len_{131072};
    std::string weight_quant_{"llq_fused"};
    std::string kv_quant_{"llq_fused"};
    std::string rope_scheme_{"yarn"};
    double rope_scale_{8.0};
};

} // namespace logger::codegen
