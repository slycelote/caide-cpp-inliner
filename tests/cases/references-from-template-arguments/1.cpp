constexpr int a = 2;
template<int n> void f() {}

using A = int;
template<typename T> struct B {};

int main() {
    f<a+a>();
    B<A> var;
    (void)var;
}

