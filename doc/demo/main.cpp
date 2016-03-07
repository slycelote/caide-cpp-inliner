#include "ascii_graphics.h"
#include "cmdparse.h"
#include <iostream>

struct Options {
    std::string name;
    int numExclamations;
#ifdef OPT_FRAME
    bool drawFrame;
#endif

    Options()
        : name("World")
        , numExclamations(1)
#ifdef OPT_FRAME
        , drawFrame(false)
#endif
    {}
};

int main(int argc, const char* argv[]) {
    Options options;
    cmdparse::CommandLineParser<Options> parser;

    try {
        parser.add("-name", &Options::name)
#ifdef OPT_FRAME
            .add("-f", &Options::drawFrame)
#endif
            .add("-c", &Options::numExclamations);

        options = parser.parse(argv + 1, argv + argc);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    const std::string greeting = std::string("Hello, ") + options.name
        + std::string(options.numExclamations, '!');

#ifdef OPT_FRAME
    if (options.drawFrame)
        ascii::outputInFrame(greeting, std::cout);
    else
        std::cout << greeting << std::endl;
#else
    std::cout << greeting << std::endl;
#endif

    return 0;
}

