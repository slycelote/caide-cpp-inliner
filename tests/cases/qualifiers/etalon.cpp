struct A {
    void f() {}
    int i = 2;
};

struct B: public A {
    using Base = A;
    void g() {
        Base::f();
    }
};

struct C: public A {
    using Base = A;
    void g() {
        (void)Base::i;
    }
};

struct D {
    void f() {}
};
struct E: private D {
    using Base = D;
    using Base::f;
};

struct F {
    static int i;
};
int F::i = 0;
struct F1: private F {
    using F::i;
};

int main() {
    B b;
    b.g();
    C c;
    c.g();
    E e;
    e.f();
    int i = F1::i;
    (void)i;
}

