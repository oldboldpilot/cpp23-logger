/**
 * The literal-safe codegen filters: escape / cpp_string, python_string / json_string, and
 * c_ident / safe_ident.
 *
 * Every control byte 0x00-0x1F and 0x7F is checked, not a sample: a filter that names \n but lets
 * \v through is the defect this test exists for.
 */

// Textual stdlib includes before the module import (see examples/fluent_codegen.cpp).
#include <cassert>
#include <cctype>
#include <iostream>
#include <string>
#include <string_view>

import logger.code_generator;

using namespace logger::codegen;

namespace {

auto isControl(unsigned char u) -> bool { return u < 0x20 || u == 0x7f; }

/// True when `body` holds no raw control byte and no unescaped double quote.
auto literalSafe(std::string_view body) -> bool {
    for (std::size_t i = 0; i < body.size(); ++i) {
        const auto u = static_cast<unsigned char>(body[i]);
        if (isControl(u)) {
            return false;
        }
        if (body[i] == '\\') {
            ++i;  // the escaped byte is never a raw control byte (checked above on the next pass)
            if (i < body.size() && isControl(static_cast<unsigned char>(body[i]))) {
                return false;
            }
            continue;
        }
        if (body[i] == '"') {
            return false;
        }
    }
    return true;
}

auto test_escape_every_control_byte() -> void {
    std::cout << "Testing escape / cpp_string over every control byte...\n";
    FilterRegistry const reg{};
    for (unsigned u = 0; u < 0x80; ++u) {
        std::string const in(1, static_cast<char>(u));
        auto const out = reg.applyFilter("cpp_string", in);
        assert(out == reg.applyFilter("escape", in));
        assert(out == escape(in));
        assert(literalSafe(out));
        if (isControl(static_cast<unsigned char>(u)) && u != '\n' && u != '\r' && u != '\t') {
            // three-digit octal: a following digit can never extend it
            std::string want = "\\";
            want += static_cast<char>('0' + ((u >> 6) & 7));
            want += static_cast<char>('0' + ((u >> 3) & 7));
            want += static_cast<char>('0' + (u & 7));
            assert(out == want);
        }
    }
    assert(escape("a\nb\rc\td") == "a\\nb\\rc\\td");
    assert(escape("q\"\\") == "q\\\"\\\\");
    assert(escape(std::string_view{"\x01" "7", 2}) == "\\0017");
    assert(escape(std::string_view{"\0", 1}) == "\\000");
    assert(escape("\x7f") == "\\177");
    assert(escape("\xc3\xa9") == "\xc3\xa9");  // UTF-8 passes through
    std::cout << "  ✓ escape / cpp_string test passed\n";
}

auto test_python_string_every_control_byte() -> void {
    std::cout << "Testing python_string / json_string over every control byte...\n";
    FilterRegistry const reg{};
    static constexpr std::string_view kHex = "0123456789abcdef";
    for (unsigned u = 0; u < 0x80; ++u) {
        std::string const in(1, static_cast<char>(u));
        auto const out = reg.applyFilter("python_string", in);
        assert(out == reg.applyFilter("json_string", in));
        assert(out == pythonString(in));
        assert(literalSafe(out));
        if (isControl(static_cast<unsigned char>(u)) && u != '\n' && u != '\r' && u != '\t' &&
            u != '\b' && u != '\f') {
            std::string want = "\\u00";
            want += kHex[u >> 4];
            want += kHex[u & 0xf];
            assert(out == want);
        }
    }
    assert(pythonString("a\nb\rc\td\be\ff") == "a\\nb\\rc\\td\\be\\ff");
    assert(pythonString("x\", \"evil\": 1") == "x\\\", \\\"evil\\\": 1");
    assert(pythonString("back\\slash") == "back\\\\slash");
    assert(pythonString(std::string_view{"\0", 1}) == "\\u0000");
    assert(pythonString("\x1f\x7f") == "\\u001f\\u007f");
    assert(pythonString("\xc3\xa9") == "\xc3\xa9");
    std::cout << "  ✓ python_string / json_string test passed\n";
}

auto test_safe_identifier() -> void {
    std::cout << "Testing c_ident / safe_ident...\n";
    FilterRegistry const reg{};
    for (auto const* f : {"c_ident", "safe_ident"}) {
        assert(reg.applyFilter(f, "") == "_");
        assert(reg.applyFilter(f, "qwen3.5-27b") == "qwen3_5_27b");
        assert(reg.applyFilter(f, "7b") == "_7b");
        assert(reg.applyFilter(f, "../../x\"; system(\"id\")") == "______x___system__id__");
        assert(reg.applyFilter(f, "\xc3\xa9") == "__");
    }
    assert(safeIdentifier("", "model", "m_") == "model");
    assert(safeIdentifier("7b", "model", "m_") == "m_7b");
    assert(safeIdentifier("ok_1", "model", "m_") == "ok_1");
    assert(toCIdentifier("") == "_");
    // never empty, never a leading digit, [A-Za-z0-9_] only -- over every single byte
    for (unsigned u = 0; u < 0x100; ++u) {
        auto const id = safeIdentifier(std::string(1, static_cast<char>(u)));
        assert(!id.empty());
        assert(std::isdigit(static_cast<unsigned char>(id[0])) == 0);
        for (char const c : id) {
            assert(std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_');
        }
    }
    std::cout << "  ✓ c_ident / safe_ident test passed\n";
}

auto test_filters_in_template() -> void {
    std::cout << "Testing the filters inside a rendered template...\n";
    CodeGenerator gen{};
    TemplateObject data{};
    data["name"] = TemplateValue{std::string{"9 \"bad\"\x01"}};
    auto const out = gen.render(
        "id={{name|safe_ident}} cpp=\"{{name|cpp_string}}\" py=\"{{name|python_string}}\"", data);
    assert(out == "id=_9__bad__ cpp=\"9 \\\"bad\\\"\\001\" py=\"9 \\\"bad\\\"\\u0001\"");
    std::cout << "  ✓ template test passed\n";
}

}  // namespace

auto main() -> int {
    test_escape_every_control_byte();
    test_python_string_every_control_byte();
    test_safe_identifier();
    test_filters_in_template();
    std::cout << "All codegen filter tests passed\n";
    return 0;
}
