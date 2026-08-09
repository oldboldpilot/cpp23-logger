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
endif()

add_test(NAME test_logger COMMAND test_logger)
add_test(NAME test_nested_templates COMMAND test_nested_templates)
add_test(NAME test_functional_templates COMMAND test_functional_templates)
