template <typename T>
T value() {
    return 42;
}

typedef int Func();

int main() {
    Func* f = value<int>;
    f();
}

