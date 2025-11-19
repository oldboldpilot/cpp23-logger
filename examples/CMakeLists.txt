add_executable(basic_usage basic_usage.cpp)
target_link_libraries(basic_usage PRIVATE logger)

add_executable(named_templates named_templates.cpp)
target_link_libraries(named_templates PRIVATE logger)
