template<typename T>
struct Type {
    using type = int;
};

template<typename T>
using Alias = typename Type<T>::type;

auto declval2() -> int*;

template <class T>
using Alias2 = decltype(*declval2());

using R2 = Alias2<int*>;

void f2(R2 x) {}

template<typename T>
auto declval3() -> T;

template <class T>
using Alias3 = decltype(*declval3<T>());

using R3 = Alias3<int*>;

void f3(R3 x) {}

int main() {
    Alias<void> a;
    (void)f2;
    (void)f3;
    return 0;
}
