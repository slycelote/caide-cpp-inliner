# This file allows users to call find_package(Clang) and pick up our targets.

get_filename_component(CLANG_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)

find_package(LLVM REQUIRED CONFIG HINTS $CLANG_CMAKE_DIR)

# Compute the installation prefix from this LLVMConfig.cmake file location.
set(CLANG_INSTALL_PREFIX /usr/lib/llvm-4.0)
set(CLANG_EXPORTED_TARGETS "clangBasic;clangLex;clangParse;clangAST;clangDynamicASTMatchers;clangASTMatchers;clangSema;clangCodeGen;clangAnalysis;clangEdit;clangRewrite;clangARCMigrate;clangDriver;clangSerialization;clangRewriteFrontend;clangFrontend;clangFrontendTool;clangToolingCore;clangTooling;clangIndex;clangStaticAnalyzerCore;clangStaticAnalyzerCheckers;clangStaticAnalyzerFrontend;clangFormat;clang;clang-format;clang-import-test;clangApplyReplacements;clangRename;clangReorderFields;clangTidy;clangTidyPlugin;clangTidyBoostModule;clangTidyCERTModule;clangTidyLLVMModule;clangTidyCppCoreGuidelinesModule;clangTidyGoogleModule;clangTidyMiscModule;clangTidyModernizeModule;clangTidyMPIModule;clangTidyPerformanceModule;clangTidyReadabilityModule;clangTidyUtils;clangChangeNamespace;clangQuery;clangMove;clangIncludeFixer;clangIncludeFixerPlugin;findAllSymbols;libclang")

# Provide all our library targets to users.
include("${CLANG_CMAKE_DIR}/ClangTargets.cmake")
