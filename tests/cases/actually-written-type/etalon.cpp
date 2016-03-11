using UsedTypeAlias = int;

template<typename T> struct A {};
template<> struct A<UsedTypeAlias> {};

typedef int usedTypedef;

template<typename T> void func() {}
template<> void func<usedTypedef>() {}


template<typename T> struct B{};

template<typename T>
using UsedTemplateAlias = B<T>;

template<typename T> struct C{};
template<typename T> struct C<UsedTemplateAlias<T>> {};


template<typename T>
struct Traits {
    using type = T;
};

template<typename T> struct D{};
template<> struct D<typename Traits<int>::type> {};

template<typename T> struct E{};

int main() {
    A<int> a; (void)a;
    func<int>();
    C<B<int>> c; (void)c;
    D<int> d; (void)d;
    E<float> e; (void)e;
}

