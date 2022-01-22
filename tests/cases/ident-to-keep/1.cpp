void f1() { }
void f2() { }
void f3() { }

namespace ns {
class C {
    C(int) { f1(); }
    void method1() {
        f2();
    }

    void method2() { }
};
}

int main() {
}

