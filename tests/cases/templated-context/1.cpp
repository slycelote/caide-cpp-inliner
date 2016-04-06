template<typename T>
struct Outer {
    using Type = T;

    template<typename S>
    struct Inner {
        int a;

        template<typename U>
        struct MoreInner { };

        template<typename U>
        struct MoreInnerUnused { };
    };
};

template<>
struct Outer<double> {
    int another;
};

int main() {
    Outer<int> a;
    Outer<int>::Type x;
    Outer<int>::Inner<char> c;
    (void)a; (void)x;
    (void)c.a;
    Outer<int>::Inner<char>::MoreInner<void> i;
    (void)i;
}

