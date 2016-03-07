#include "ascii_graphics.h"

namespace ascii {

void outputInFrame(const std::string& text, std::ostream& out) {
    const std::size_t textLength = text.length();
    const std::string astericsLine(textLength + 4, '*');
    out << astericsLine << "\n"
        << "* " << text << " *\n"
        << astericsLine << std::endl;
}

}

