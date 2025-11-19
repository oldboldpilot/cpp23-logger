add_executable(test_logger test_logger.cpp)
target_link_libraries(test_logger PRIVATE logger)

add_executable(test_nested_templates test_nested_templates.cpp)
target_link_libraries(test_nested_templates PRIVATE logger)

add_test(NAME test_logger COMMAND test_logger)
add_test(NAME test_nested_templates COMMAND test_nested_templates)
