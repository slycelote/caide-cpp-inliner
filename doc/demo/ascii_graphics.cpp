#include "ascii_graphics.h"

namespace ascii {

void outputInFrame(const std::string& text, std::ostream& out) {
    const std::size_t textLength = text.length();
    const std::string asteriscsLine(textLength + 4, '*');
    out << asteriscsLine << "\n"
        << "* " << text << " *\n"
        << asteriscsLine << std::endl;
}

}

