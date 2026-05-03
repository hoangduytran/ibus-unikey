enable_testing()
add_executable(ukengine_test ukengine_test.cpp)
target_link_libraries(ukengine_test PRIVATE ukengine)
add_test(NAME ukengine_test COMMAND ukengine_test)

# Optional GTK macro tests (require src/setup/macro_*.cpp helpers present in tree).
if(EXISTS "${CMAKE_SOURCE_DIR}/src/setup/macro_file_io.cpp")
    add_subdirectory(macros)
endif()
