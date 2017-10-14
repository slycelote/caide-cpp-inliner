struct A {
    int a, b, c, d;
    bool e, f, g;
    static long h, i;
};

long A::h = 1, A::i = 2;

int main() {
    A x;
    x.b = x.c = A::h;
    x.g = false;
}
