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

namespace test8 {
    template <typename... Ts>
    constexpr bool AlwaysTrue = true;

    template <typename... Ts>
    struct Foo {
    };

    template <>
    struct Foo<int> {
        using type = int;
    };

    template <typename T, typename U = Foo<T>::type>
        requires AlwaysTrue<U>
    struct Bar
    {
    };

    int main() {
        Bar<int> x;
        return 0;
    }
}

namespace test9 {
    template <typename... Ts>
    constexpr bool AlwaysTrue = true;

    template <typename T>
    struct S1 { };

    template <>
    struct S1<int> {
        using type = int;
    };

    template <typename... Ts>
    struct Foo { };

    template <typename T>
        requires AlwaysTrue<typename S1<T>::type>
    struct Foo<T>
    { };

    int main() {
        Foo<int> x;
        return 0;
    }
}

namespace test10 {
    template <typename T>
    struct S1 { };

    template <>
    struct S1<int> {
        using type = long;
    };

    template <typename T, typename S1<T>::type V>
    struct S2
    { };

    int main() {
        S2<int, 1l> x;
        return 0;
    }
}

namespace test11 {
    template <typename... Ts>
    struct S1{};

    template<>
    struct S1<double, int> {
        constexpr static bool value = true;
    };

    template <typename... Ts>
    concept Concept1 = S1<Ts...>::value;

    template <typename... Ts>
    void f()
    {
        Concept1<Ts...> auto x = 1.0;
    }

    int main() {
        f<int>();
        return 0;
    }
}

namespace test12 {
    template <typename... Ts>
    struct Foo {
    };

    template <>
    struct Foo<int> {
        using type = int;
    };

    template <typename... Ts>
    struct Bar {
    };

    template <>
    struct Bar<int> {
        using type = int;
    };

    template <typename T, typename U = Foo<T>::type, typename V = Bar<U>::type>
    struct S {};

    int main() {
        S<int>();
        return 0;
    }
}

namespace test13 {
    template <typename... Ts>
    struct Foo {
    };

    template <>
    struct Foo<int> {
        using type = int;
    };

    template <typename... Ts>
    struct Bar {
    };

    template <>
    struct Bar<int> {
        using type = int;
    };

    template <typename T, typename U = Foo<T>::type, typename V = Bar<U>::type>
    using S = T;

    int main() {
        S<int> x;
        (void)x;
        return 0;
    }
}

namespace test14 {
    template <typename T>
    struct S {
        using type = long;
    };

    template <typename U, typename T, typename S<T>::type V>
    concept C = true;

    int main() {
        C<double, 1l> auto x = 1;
        return 0;
    }
}

namespace test15 {
    template <typename T>
    struct S {
        using type = long;
    };

    template <typename T, typename U, typename V = S<U>::type>
    concept C = true;

    int main() {
        C<int> auto x = 1;
        return 0;
    }
}

int main() {
    f1<S1>();
    f2<double>(S2{}, 0);

    S3<int> s3;
    f3(s3);

    C4<S4<int>> c4;
    C5<S5<int>> c5;
    C6<int, void> c6;
    test8::main();
    test9::main();
    test10::main();
    test11::main();
    test12::main();
    test13::main();
    test14::main();
    test15::main();
}
