enable_testing()
add_executable(ukengine_test ukengine_test.cpp)
target_link_libraries(ukengine_test PRIVATE ukengine)
add_test(NAME ukengine_test COMMAND ukengine_test)
