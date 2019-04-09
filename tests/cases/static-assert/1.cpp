namespace case1 {
    using int2 = int;
    static_assert(int2{} == 0, "");

    constexpr int c_zero = 0;
    static_assert(c_zero == 0, "");

    template<int n>
    int func() {
        static_assert(n != 0, "");
        return n;
    }

    void main() {
        int n = func<1>();
        (void)n;
    }
}

int main() {
    case1::main();
}

