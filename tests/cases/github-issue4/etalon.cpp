template<class T> class A;

template<class T>
class A {
public:
    template<typename U>
    void f(U) {}
};

int main() {
    A<int> a;
    a.f(1);
}

