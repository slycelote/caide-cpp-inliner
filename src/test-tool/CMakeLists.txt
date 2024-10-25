add_executable(test-tool test-tool.cpp)

target_link_libraries(test-tool caideInliner)

set(tests_dir "${CMAKE_SOURCE_DIR}/../tests/cases")
set(tests_temp_dir "${CMAKE_SOURCE_DIR}/../tests/temp")
set(clang_options_file "${CMAKE_CURRENT_BINARY_DIR}/clangOptions.txt")

add_custom_command(TARGET test-tool POST_BUILD
    COMMAND test-tool "${tests_temp_dir}" "--prepare" "${clang_options_file}"
    COMMENT "Detecting compilation options")

# To run tests: make test-tool && ctest
# To run a specific test: ctest -R <test name>
# For verbose output: ctest --verbose

set(test_list actually-written-type alias-in-template-argument base-class-of-template base-initializers caide-concept-comment delayed-parsing enums friends github-issue17 github-issue4 ident-to-keep include-option-std include-option-user inheriting-ctor inliner1 inliner2 inliner3 line-directives macros merge-namespaces merge-namespaces-2 pull-headers-up qualifiers references-from-template-arguments remove-comments remove-namespaces remove-template-functions remove-type-alias sizeof static-assert std-namespace stl template-alias templated-context template-friend track-parent-decls ull unused-fields using-declarations)

function(add_test_directory test_name)
    add_test(NAME ${test_name}
        COMMAND test-tool "${tests_temp_dir}" "${clang_options_file}" "${tests_dir}/${test_name}")
    set_tests_properties(${test_name} PROPERTIES REQUIRED_FILES "${clang_options_file}")
endfunction()

foreach(test_name IN LISTS test_list)
    add_test_directory(${test_name})
endforeach()

if(LLVM_PACKAGE_VERSION VERSION_GREATER "9")
    add_test_directory(concept-constraints)
endif()

if(LLVM_PACKAGE_VERSION VERSION_GREATER "15")
    add_test_directory(github-issue8)
endif()
