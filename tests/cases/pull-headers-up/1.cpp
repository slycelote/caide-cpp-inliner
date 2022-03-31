#include <myvector.h>
void f(mystd::vector& v) {}

#include <mymap.h>
int main() {
    mystd::map map;
    mystd::vector v;
    f(v);
}
