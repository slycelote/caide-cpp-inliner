#include "../caideInliner.hpp"

#include <iostream>
#include <stdexcept>
#include <string>


using namespace std;

int main(int argc, const char* argv[]) {
    try {
        caide::CppInliner inliner("./caide-tmp");
        vector<string> files;

        int i = 1;
        static const string endOfClangOptions = "--";
        while (endOfClangOptions != argv[i]) {
            inliner.clangCompilationOptions.push_back(argv[i]);
            ++i;
        }

        for (++i; i < argc; ++i) {
            static const string flagKeep = "-k";
            if (flagKeep == argv[i]) {
                ++i;
                if (i < argc)
                    inliner.macrosToKeep.push_back(argv[i]);
            } else
                files.push_back(argv[i]);
        }

        inliner.inlineCode(files, "./caide-tmp/result.cpp");
    } catch (const exception& e) {
        cerr << e.what() << endl;
        return 1;
    }

    return 0;
}

