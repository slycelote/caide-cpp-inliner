template<typename Func>
struct F {
    template<typename T>
    F(const T& f) {}
};

void used() {}

int main() {
    F<void(int)> f = [&](int i) {
        used();
    };
    return 0;
}

