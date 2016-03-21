#include <mystd.h>
using namespace mystd;
void f(vector& v) {}
int main() {
    vector v;
    f(v);
    return 0;
}
