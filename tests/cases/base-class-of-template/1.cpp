struct base {};

template <typename A>
struct templ : base {};

template <>
struct templ<void> {};

struct unused_base {};
template <>
struct templ<double> : unused_base {};

int main() {
    templ<void> x;
    (void)x;
}

