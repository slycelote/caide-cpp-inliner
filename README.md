# Caide C++ inliner

[![Build
status](https://travis-ci.org/slycelote/caide-cpp-inliner.svg)](https://travis-ci.org/slycelote/caide-cpp-inliner)

*Caide C++ inliner* transforms a C++ program consisting of multiple source
files and headers into a single self-contained source file without any
external dependencies (except for standard system headers). Unused code is not
included in the resulting file.

## Demo

The directory [doc/demo](./../../tree/master/doc/demo) contains a small sample
application. We will write a program using caideInliner library that processes
the source code of this application:


    #include "caideInliner.hpp"
    #include <iostream>
    #include <stdexcept>
    #include <string>
    #include <vector>

    int main() {
        using namespace std;
        try {
            // (1)
            const string tempDirectory = "/tmp";
            caide::CppInliner inliner(tempDirectory);
            // (2)
            inliner.clangCompilationOptions.push_back("-std=c++11");
            inliner.clangCompilationOptions.push_back("-I");
            inliner.clangCompilationOptions.push_back(".");
            // (3)
            inliner.clangCompilationOptions.push_back("-isystem");
    #ifdef _WIN32
            inliner.clangCompilationOptions.push_back("C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.10150.0\\ucrt");
    #else
            inliner.clangCompilationOptions.push_back("../../src/clang/lib/Headers");
    #endif

            // (4)
            vector<string> files;
            files.push_back("main.cpp");
            files.push_back("ascii_graphics.cpp");

            // (5)
            inliner.inlineCode(files, "result.cpp");
        } catch (const exception& e) {
            cerr << e.what() << endl;
            return 1;
        }
        return 0;
    }


The above code, after including the [caideInliner
header](src/caideInliner.hpp):

1. specifies a directory for temporary files (it must already exist)
2. sets up compilation options (they may vary depending on the application)
3. adds additional directories containing system header files. It's either a
   hardcoded path for VS 2015, or a path to built-in clang headers (available
   in clang source tree included in this repository). You may need to tweak
   these options further depending on your system.
4. lists all c++ source files of the application.
5. finally, runs the inliner and outputs the result in a single c++ file.

Compile and run this code. (You will need to link against caideInliner library
and several clang and LLVM static libraries.) Alternatively, a [simple
command-line utility](./../../tree/master/src/cmd) is included that you can
run like this: `./cmd -std=c++11 -I . -isystem ../../src/clang/lib/Headers/ --
main.cpp ascii_graphics.cpp`.

Inspect the resulting code in the file `result.cpp`. Note how unused parts of
the code (e.g. one of the constructors of `CommandLineParserException` or
inactive preprocessor blocks from `main.cpp`) were eliminated.

Now add a preprocessor definition `OPT_FRAME`:

            inliner.clangCompilationOptions.push_back("-DOPT_FRAME");

and rerun the program. Observe how the output file changes accordingly.


## Build

You will need [CMake](https://cmake.org) and a relatively new compiler (tested
with g++ 4.8 and VS 2015). From an empty directory execute the following
command: `cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release /path/to/src`.
Replace "Unix Makefiles" with "Visual Studio 12 2013" or another
[generator](https://cmake.org/cmake/help/v3.0/manual/cmake-generators.7.html)
suitable for your platform. Replace the last argument with the full path to
[src](./../../tree/master/src) directory. You can also use
[cmake-gui](https://cmake.org/runningcmake/).

Once build files are generated, build as usual (run `make`, open the VS
solution etc.)

By default all necessary parts of libclang and LLVM will be built from
scratch. This may take up to 30 minutes on a weak machine. If you have those
libraries installed in the system (in Linux, search for packages
libclang-3.6-dev and llvm-3.6-dev or similar), you can use them instead by
passing `-DCAIDE_USE_SYSTEM_CLANG=ON` option to cmake. However, **it's not
recommended**.

When the build is done, run `ctest` to execute the test suite.


## Documentation

Refer to Doxygen comments in [caideInliner header](src/caideInliner.hpp).


## Known issues

* Non-standard features, such as defining a function without return type, are
  not supported.
* Defining a class and a variable of the same class simultaneously (`struct A
  {} a;`) is not supported.
* Unused global variables are removed. This is fine, unless you use such a
  variable for side effects. In that case, mark the variable with a comment
  `/// caide keep` or `/** caide keep **/` (note the triple slash and the
  double asterisk). For example,

        struct SideEffect {...};
        /// caide keep
        SideEffect instance;

  In general, if you find that a declaration is removed incorrectly, mark this
  declaration with `caide keep` (and file an issue :)).


