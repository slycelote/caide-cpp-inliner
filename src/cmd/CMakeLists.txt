add_executable(cmd cmd.cpp)

target_link_libraries(cmd caideInliner)
target_link_libraries(cmd ${clang_libs})
target_link_libraries(cmd ${llvm_libs})

