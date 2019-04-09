namespace case1 {
    template<int n>
    int func() {
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

