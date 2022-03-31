add_executable(test-tool test-tool.cpp)

target_link_libraries(test-tool caideInliner)

set(tests_dir "${CMAKE_SOURCE_DIR}/../tests/cases")
set(tests_temp_dir "${CMAKE_SOURCE_DIR}/../tests/temp")
set(clang_options_file "${CMAKE_CURRENT_BINARY_DIR}/clangOptions.txt")

add_custom_command(TARGET test-tool POST_BUILD
    COMMAND test-tool "${tests_temp_dir}" "--prepare" "${clang_options_file}"
    COMMENT "Detecting compilation options")

file(GLOB test_name_list RELATIVE "${tests_dir}" "${tests_dir}/*")
list(SORT test_name_list)

# To run tests: make test-tool && ctest
# For verbose output: `CTEST_OUTPUT_ON_FAILURE=1 ctest' or `ctest --verbose'
# After adding a new test case, re-run cmake
foreach(test_name IN LISTS test_name_list)
    add_test(NAME ${test_name}
        COMMAND test-tool "${tests_temp_dir}" "${clang_options_file}" "${tests_dir}/${test_name}")
    set_tests_properties(${test_name} PROPERTIES REQUIRED_FILES "${clang_options_file}")
endforeach()

