struct A {};

using Type = A;
struct B: A {
    B(): Type() {}
};

int main() {
    B b;
    (void)b;
}

