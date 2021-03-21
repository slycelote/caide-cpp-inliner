//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#include "clang_version.h"
#include "util.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <clang/AST/ASTConsumer.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>

using std::string;
using std::vector;

using namespace clang;

namespace caide {
namespace internal {

namespace {

string trim(const string& s) {
    std::size_t i = s.find_first_not_of(" ");
    std::size_t j = s.find_last_not_of(" ");
    if (i != string::npos && j != string::npos && i <= j)
        return s.substr(i, j - i + 1);
    return s;
}

bool startsWith(const string& s, const string& prefix) {
    return s.length() >= prefix.length() &&
        std::mismatch(prefix.begin(), prefix.end(), s.begin()).first == prefix.end();
}

bool endsWith(const string& s, const string& prefix) {
    return s.length() >= prefix.length() &&
        std::mismatch(prefix.rbegin(), prefix.rend(), s.rbegin()).first == prefix.rend();
}

string pathConcat(const string& directory, const string& fileName) {
    return directory + "/" + fileName;
}

class DetectOptionsFrontendAction: public ASTFrontendAction {
    virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance&, StringRef /*file*/) override {
        return std::unique_ptr<ASTConsumer>(new ASTConsumer());
    }
};

class DetectOptionsFrontendActionFactory: public clang::tooling::FrontendActionFactory {
public:
#if CAIDE_CLANG_VERSION_AT_LEAST(10, 0)
    std::unique_ptr<FrontendAction> create() override {
        return std::make_unique<DetectOptionsFrontendAction>();
    }
#else
    FrontendAction* create() override {
        return new DetectOptionsFrontendAction();
    }
#endif
};

bool testOptions(const vector<string>& compilationOptions, const string& cppFile) {
    std::unique_ptr<clang::tooling::FixedCompilationDatabase> compilationDatabase(
        createCompilationDatabaseFromCommandLine(compilationOptions));

    vector<string> sources{cppFile};
    DetectOptionsFrontendActionFactory factory;
    clang::tooling::ClangTool tool(*compilationDatabase, sources);
    return tool.run(&factory) == 0;
}

} // anonymous namespace

vector<string> detectClangOptions(const string& temporaryDirectory) {
    vector<string> gccLikeCompilers;
    if (const char* cxx = ::getenv("CXX"))
        gccLikeCompilers.push_back(cxx);
    vector<string> options{"g++", "clang++"};
    for (const auto& s : options) {
        if (gccLikeCompilers.end() == std::find(gccLikeCompilers.begin(), gccLikeCompilers.end(), s)) {
            gccLikeCompilers.push_back(s);
        }
    }

    const string emptySourceFile = pathConcat(temporaryDirectory, "empty.cpp");
    const string outputExeFile = pathConcat(temporaryDirectory, "detect.exe");
    const string gccLogFile = pathConcat(temporaryDirectory, "gcclog.txt");
    const string detectSourceFile = pathConcat(temporaryDirectory, "detect.cpp");
    (void)std::ofstream(emptySourceFile.c_str());
    std::ofstream detectFile(detectSourceFile.c_str());
    detectFile <<
        "#include <cstdlib>\n#include <csignal>\n#include <csetjmp>\n#include <cstdarg>\n"
        "#include <typeinfo>\n#include <bitset>\n#include <functional>\n#include <utility>\n"
        "#include <ctime>\n#include <cstddef>\n#include <new>\n#include <memory>\n"
        "#include <climits>\n#include <cfloat>\n#include <limits>\n#include <exception>\n"
        "#include <stdexcept>\n#include <cassert>\n#include <cerrno>\n#include <cctype>\n"
        "#include <cwctype>\n#include <cstring>\n#include <cwchar>\n#include <string>\n"
        "#include <vector>\n#include <deque>\n#include <list>\n#include <set>\n#include <map>\n"
        "#include <stack>\n#include <queue>\n#include <iterator>\n#include <algorithm>\n"
        "#include <cmath>\n#include <complex>\n#include <valarray>\n#include <numeric>\n"
        "#include <locale>\n#include <clocale>\n#include <iosfwd>\n#include <ios>\n#include <istream>\n"
        "#include <ostream>\n#include <iostream>\n#include <fstream>\n#include <sstream>\n"
        "#include <iomanip>\n#include <streambuf>\n#include <cstdio>\n"
        "int main() { return 0; }\n";
    detectFile.close();

    for (const auto& cxx: gccLikeCompilers) {
        // TODO: escape whitespace and quotes.
        // TODO: use correct locale somehow.
        std::stringstream command;
        command << cxx << " -x c++ -c -v " << emptySourceFile << " -o " << outputExeFile << " 2>" << gccLogFile;
        std::system(command.str().c_str());
        std::ifstream logFile(gccLogFile.c_str());
        string line;
        bool isSearchList = false;
        vector<string> compilationOptions;
        while (std::getline(logFile, line)) {
            if (startsWith(line, "End of search list"))
                break;

            if (endsWith(line, "search starts here:"))
                isSearchList = true;
            else if (isSearchList) {
                compilationOptions.push_back("-isystem");
                compilationOptions.push_back(trim(line));
            }
        }

        if (compilationOptions.empty())
            continue;

        compilationOptions.push_back("-nostdlibinc");
        compilationOptions.push_back("-nobuiltininc");
        if (testOptions(compilationOptions, detectSourceFile))
            return compilationOptions;

        compilationOptions.pop_back();
        if (testOptions(compilationOptions, detectSourceFile))
            return compilationOptions;
    }

    return {}; // let clang determine the options automatically
}

}
}
