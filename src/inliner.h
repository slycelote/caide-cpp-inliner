#include <vector>
#include <string>
#include <set>

// First inliner stage: inline included headers
class Inliner {
public:
    explicit Inliner(const std::vector<std::string>& clangCommandLineOptions);

    std::string doInline(const std::string& cppFile);

private:
    std::vector<std::string> cmdLineOptions;
    std::set<std::string> includedHeaders;
    std::vector<std::string> inlineResults;
};

