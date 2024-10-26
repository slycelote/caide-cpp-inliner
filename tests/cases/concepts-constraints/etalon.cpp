template <typename T>
concept Add = requires(T a, T b) {
    { a + b };
};

template <typename T>
    requires Add<T>
T add(T a, T b) {
    return a + b;
}

int main() {
    return add(-5, 5);
}

