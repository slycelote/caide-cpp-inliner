// Unused1
// Multi-line
// comment
struct Unused1 {};

// Used
struct Used1 {
    // unusedMethod: comment for declaration
    void unusedMethod();

    // UnusedTypedef
    typedef int UnusedTypedef;
};

/* Unused2
 Multi-line
 comment
 */
struct Unused2 {};

// unusedMethod: comment for definition
void Used1::unusedMethod() {
}

int main() {
    Used1 u;
    return 0;
}

