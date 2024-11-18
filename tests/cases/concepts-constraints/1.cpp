template <typename T>
concept Add = requires(T a, T b) {
    { a + b };
};

template <typename T>
concept Subtract = requires(T a, T b) {
    { a - b };
};

template <typename T>
    requires Add<T>
T add(T a, T b) {
    return a + b;
}

template <typename T>
concept Add2 = requires(T a, T b) { { a + b }; };

template <Add2 T>
T add2(T a) { return a; }

template <typename T>
concept Add3 = requires(T a, T b) { { a + b }; };

auto add3(Add3 auto a) { return a; }

int main() {
    add(-5, 5);
    add2(1);
    add3(1);
}

