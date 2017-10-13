struct A {
    A(int) {}
};

struct B : A {
    using A::A;
};

int main() {
    B(52);
    return 0;
}
