using X = int;
using Y = int;

template <typename T>
void f() { }

template <typename T>
class C {
};

int main() {
    f<X>();
    C<Y>* pc;
}

