//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#pragma once

#include <string>
#include <vector>

/// \file caideInliner.hpp
/// \brief C++ interface for caide inliner API


namespace caide {

/// \brief C++ code inliner and unused code remover
///
/// The C++ inliner transforms a program implemented as multiple C++ source files
/// and headers into a single-file program. Only code reachable from main function
/// is kept in the output file.
///
/// Faily complex programs are supported, including programs using template metaprogramming
/// and modern C++ features. That said, don't try to inline Boost headers.
///
/// \sa inlineCode()
class CppInliner {
public:
    /// \brief Create an instance of C++ inliner
    /// \param temporaryDirectory path to a directory where intermediate
    /// files will be written. The directory must exist.
    ///
    /// \sa clangCompilationOptions
    /// \sa macrosToKeep
    /// \sa maxConsequentEmptyLines
    explicit CppInliner(const std::string& temporaryDirectory);


    /// \brief Generate a single-file C++ program equivalent to the multiple-file program.
    /// \param cppFilePaths full paths of all C++ files of a program
    /// \param outputFilePath path to a file where the inlined program will be written
    ///
    /// All input C++ files and included user headers will be combined into a single C++ file,
    /// and only code reachable from main function will be kept.
    void inlineCode(const std::vector<std::string>& cppFilePaths,
                    const std::string& outputFilePath) const;


    /// \brief clang compilation options (see http://clang.llvm.org/docs/CommandGuide/clang.html
    /// and http://clang.llvm.org/docs/UsersManual.html)
    ///
    /// Most commonly used options are:
    ///
    /// \li `-I` to setup include search paths
    /// \li `-isystem` to setup system include search paths. In particular, you will likely need
    ///     to add builtin clang includes as a system path. See this link:
    ///     http://clang.llvm.org/docs/LibTooling.html#libtooling-builtin-includes
    /// \li `-D` for custom symbol definitions
    /// \li `-std=c++11` to specify a version of C++ standard
    /// \li `-v` for verbose output
    ///
    /// Other options you may need if they are not determined automatically (e.g. if
    /// the library was built in MinGW but you want to use Visual Studio headers, or if
    /// the library was built in a different Linux distribution):
    ///
    /// \li `-target` to specify compilation target (e.g., i386-pc-mingw32, i386-pc-windows-msvc,
    ///     i386-pc-linux)
    /// \li `-fmsc-version` to specify a version of Microsoft compiler installed in the system
    ///
    /// Default value is empty.
    std::vector<std::string> clangCompilationOptions;


    /// \brief the list of macro definitions whose values should be assumed unknown
    ///
    /// The inliner will remove inactive preprocessor blocks. Sometimes it is
    /// not desirable. For example, if you have a Linux-specific code path
    /// and a Windows-specific code path, you want to always keep both of them.
    /// This parameter contains macro names, references to which in an
    /// #if or #ifdef condition will protect the corresponding preprocessor branches
    /// from being removed.
    ///
    /// Default value is a list of common OS-specific and compiler-specific definitions.
    ///
    /// \note The reference is checked by substring search. So, if this parameter
    /// contains a string "_WIN32" and a preprocessor condition is `#ifdef HAVE_WIN32`
    /// or `#if defined(_WIN32) || defined(_WIN64)`, then preprocessor branches
    /// following this condition will not be removed.
    ///
    /// \note Unused code inside an inactive preprocessor block is not removed.
    std::vector<std::string> macrosToKeep;


    /// \brief limit on the number of consecutive empty lines in the output file
    ///
    /// When unused code is removed, it leaves behind large blocks of empty lines.
    /// This parameter controls the number of consequent empty lines that is kept
    /// in the output file.
    ///
    /// Default value is 2. If the parameter is negative, empty lines are not removed.
    int maxConsequentEmptyLines;

private:
    const std::string temporaryDirectory;
};

} // namespace caide

