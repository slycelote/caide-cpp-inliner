struct S1 { using type = int; };

template<class T, class U = typename T::type>
void f1() { }

struct S2 {
    using type = int;
    using type2 = int;
};

template<class T1, class T2=typename T1::something, class U = typename T2::type>
void f2(T2, typename T2::type2 x) { }

template <typename T>
struct S3 {
    using type = T;
};

template <class T, class U = typename T::type>
void f3(T&) { }

template <typename T>
struct S4 { using type = T; };

template <class T, class U = typename T::type>
class C4 {};

template <typename T>
struct S5 { using type = T; };

template <class T, class U = typename T::type>
using C5 = T;

template <typename T>
struct S6 { using type = void; };

template <typename T, typename S>
struct C6 {};

template <typename T>
struct C6<T, typename S6<T>::type> { };

template <typename... Ts>
class C7 {
    template <int N, bool = (N < sizeof...(Ts))>
    using X1 = int;

    template <typename T>
    using X2 = X1<sizeof(T)>;
};

template <typename... Ts>
class C7<C7<Ts...>> {
    template <int N, bool = (N < sizeof...(Ts))>
    using X1 = int;

    template <typename T>
    using X2 = X1<sizeof(T)>;
};

template <>
class C7<int, int> {
    template <typename... Ts>
    class C {
        template <int N, bool = (N < sizeof...(Ts))>
        using X1 = int;

        template <typename T>
        using X2 = X1<sizeof(T)>;
    };

    template <typename... Ts>
    void f() {
        using X1 = int;
        using X2 = X1;
    };
};

template <>
void C7<int, int>::f<int>() {
    using X1 = int;
    using X2 = X1;
}

int main() {
    f1<S1>();
    f2<double>(S2{}, 0);

    S3<int> s3;
    f3(s3);

    C4<S4<int>> c4;
    C5<S5<int>> c5;
    C6<int, void> c6;
}
