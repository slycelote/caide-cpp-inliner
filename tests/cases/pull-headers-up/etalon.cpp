#include <myvector.h>
#include <mymap.h>
void f(mystd::vector& v) {}

int main() {
    mystd::map map;
    mystd::vector v;
    f(v);
}
