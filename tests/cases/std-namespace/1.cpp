#include <mystd.h>
using namespace mystd;
void f(vector& v) {}
int main() {
    using namespace mystd;
    vector v;
    f(v);
    return 0;
}
