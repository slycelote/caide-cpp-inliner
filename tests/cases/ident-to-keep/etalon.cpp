void f1() { }
void f2() { }

namespace ns {
class C {
    C(int) { f1(); }
    void method1() {
        f2();
    }

};
}

int main() {
}


