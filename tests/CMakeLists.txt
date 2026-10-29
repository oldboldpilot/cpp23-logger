# NOTE: these tests express their checks with assert(). assert() expands to a
# no-op when NDEBUG is defined, which CMake adds for Release/MinSizeRel/
# RelWithDebInfo builds — that would make the whole suite vacuous (it would only
# catch crashes, not failed checks). Force assertions live regardless of build
# type by undefining NDEBUG for every test target.
add_executable(test_logger test_logger.cpp)
target_link_libraries(test_logger PRIVATE logger)
target_compile_options(test_logger PRIVATE -UNDEBUG)

add_executable(test_nested_templates test_nested_templates.cpp)
target_link_libraries(test_nested_templates PRIVATE logger)
target_compile_options(test_nested_templates PRIVATE -UNDEBUG)

add_executable(test_functional_templates test_functional_templates.cpp)
target_link_libraries(test_functional_templates PRIVATE logger)
target_compile_options(test_functional_templates PRIVATE -UNDEBUG)

# Test std module binary compatibility with logger
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND TARGET build_logger_std_modules)
    add_executable(test_std_module test_std_module.cpp)
    target_link_libraries(test_std_module PRIVATE logger)

    # Ensure PCM files are built first
    add_dependencies(test_std_module build_logger_std_modules)

    # CRITICAL: Must use the EXACT same flags as std.pcm was built with.
    #
    # test_std_module.cpp does `import std;` directly in its own source, and
    # this target has no CXX_SCAN_FOR_MODULES / FILE_SET CXX_MODULES of its
    # own (only the `logger` library target does, which is why `import
    # logger;` in the same file needs no equivalent flag here — CMake
    # propagates that BMI location automatically via target_link_libraries).
    # CMAKE_CXX_MODULE_STD is forced OFF project-wide (see the top of
    # CMakeLists.txt), so there is no CMake-managed std-module target to link
    # against either. std.pcm here is a hand-built BMI produced by a raw
    # add_custom_command (see LOGGER_STD_PCM's own definition), invisible to
    # CMake's module dependency graph — the only way for this TU to resolve
    # `import std;` against it is to tell the compiler explicitly where to
    # look. LOGGER_STD_PCM_DIR is exported by the top-level CMakeLists.txt as
    # the single source of truth for that directory (derived from STD_PCM's
    # own path, not a separately-hardcoded literal) — this is the correct
    # consumer-side site for that dependency, not a stray leftover.
    target_compile_options(test_std_module PRIVATE
        ${LOGGER_STD_PCM_FLAGS}
        -fprebuilt-module-path=${LOGGER_STD_PCM_DIR}
        -UNDEBUG
    )

    # Link with libc++ explicitly for std module support
    target_link_directories(test_std_module PRIVATE /usr/local/lib)
    target_link_libraries(test_std_module PRIVATE c++ c++abi)

    add_test(NAME test_std_module COMMAND test_std_module)

    # JSONL sink, rotation, console stream and the level race. It does `import std;`, so it
    # needs the exact flags and BMI directory test_std_module uses (see the note above).
    add_executable(test_jsonl_sink test_jsonl_sink.cpp)
    target_link_libraries(test_jsonl_sink PRIVATE logger)
    add_dependencies(test_jsonl_sink build_logger_std_modules)
    target_compile_options(test_jsonl_sink PRIVATE
        ${LOGGER_STD_PCM_FLAGS}
        -fprebuilt-module-path=${LOGGER_STD_PCM_DIR}
        -UNDEBUG
    )
    target_link_directories(test_jsonl_sink PRIVATE /usr/local/lib)
    target_link_libraries(test_jsonl_sink PRIVATE c++ c++abi)
    add_test(NAME test_jsonl_sink COMMAND test_jsonl_sink)
    set_tests_properties(test_jsonl_sink PROPERTIES TIMEOUT 120)
else()
    message(STATUS "test_jsonl_sink not registered: it needs Clang and the std module PCM")
endif()

add_test(NAME test_logger COMMAND test_logger)
add_test(NAME test_nested_templates COMMAND test_nested_templates)
add_test(NAME test_functional_templates COMMAND test_functional_templates)

# ============================================================================
# The documentation citation gate's OWN gate
# ============================================================================
#
# `tools/check_docs_claims.py` is this repository's documentation gate: every code name a
# Markdown file cites in backticks must exist in the tracked sources. `config/cpp_details.txt`
# and `config/update_policy.txt` both name it, and `tools/test_check_docs_claims.sh` is the
# self-test that proves THIS COPY of the gate still detects what it claims to.
#
# Until now the self-test was carried here and reachable only by hand, so it passed whenever
# somebody remembered to type it -- which is indistinguishable from a gate that passes, and is
# exactly the defect the citation gate exists to catch in prose. sensen and
# ThinButQuickWebFramework register it as the ctest `check_docs_claims`; this is the same
# registration, so the gate runs wherever the suite runs.
#
# `PROJECT_IS_TOP_LEVEL` is the guard. This repository is a submodule of sensen,
# ThinButQuickWebFramework and OptionsAndFuturesCalculator, and each of those registers its
# OWN copy of the gate against its OWN documentation. Without the guard a superproject's ctest
# would also run this one, gating cpp23-logger's docs from a suite that is not about them.
#
# No build target and no WORKING_DIRECTORY: the script resolves its own directory and builds
# its fixtures under TMPDIR, so it passes from any cwd.
if(PROJECT_IS_TOP_LEVEL)
    add_test(NAME check_docs_claims
             COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_check_docs_claims.sh)
    set_tests_properties(check_docs_claims PROPERTIES TIMEOUT 120 LABELS "docs;policy")
endif()
