template<typename T>
void funct();

void func();

template<typename T>
struct A;

template<typename T>
struct B;

template<typename T>
struct C;

template<typename T>
class D {
    static void f();

    friend struct A< B<T> >;

    template<typename>
    friend struct C;

    template<typename>
    friend void funct();

    friend void func();
};

template<typename T>
struct C {
    static void f() {
        D<T>::f();
    }
};

template<typename T>
struct A {};

template<>
struct A< B<int> > {
public:
    template<typename T>
    static void f() {
        D<T>::f();
    }
};

void func() {
    D<int>::f();
}

template<typename T>
void funct() {
    D<T>::f();
}

int main () {
    A< B<int> >::f<int>();
    C<int>::f();
    funct<int>();
    func();
}
