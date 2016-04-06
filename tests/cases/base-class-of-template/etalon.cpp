struct base {};

template <typename A>
struct templ : base {};

template <>
struct templ<void> {};

int main() {
    templ<void> x;
    (void)x;
}

