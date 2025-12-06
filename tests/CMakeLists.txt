add_executable(test_logger test_logger.cpp)
target_link_libraries(test_logger PRIVATE logger)

add_executable(test_nested_templates test_nested_templates.cpp)
target_link_libraries(test_nested_templates PRIVATE logger)

add_executable(test_functional_templates test_functional_templates.cpp)
target_link_libraries(test_functional_templates PRIVATE logger)

# Test std module binary compatibility with logger
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND TARGET build_logger_std_modules)
    add_executable(test_std_module test_std_module.cpp)
    target_link_libraries(test_std_module PRIVATE logger)

    # Ensure PCM files are built first
    add_dependencies(test_std_module build_logger_std_modules)

    # CRITICAL: Must use the EXACT same flags as std.pcm was built with
    target_compile_options(test_std_module PRIVATE
        ${LOGGER_STD_PCM_FLAGS}
        -fprebuilt-module-path=${LOGGER_STD_PCM_DIR}
    )

    # Link with libc++ explicitly for std module support
    target_link_directories(test_std_module PRIVATE /usr/local/lib)
    target_link_libraries(test_std_module PRIVATE c++ c++abi)

    add_test(NAME test_std_module COMMAND test_std_module)
endif()

add_test(NAME test_logger COMMAND test_logger)
add_test(NAME test_nested_templates COMMAND test_nested_templates)
add_test(NAME test_functional_templates COMMAND test_functional_templates)
