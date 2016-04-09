constexpr int a = 2;
constexpr int b = 3;
constexpr int c = 4;
template<int n> void f() {}

template<> void f<b+b>() {}

using A = int;
template<typename T> struct B {};

template<int n> struct C {};

template<> struct C<c+c> {};

int main() {
    f<a+a>();
    f<6>();
    B<A> var;
    (void)var;
    C<8> var2;
    (void)var2;
}

