template <typename T>
int I1 = 1;

template <typename T>
int I2 = 2;

struct S1 {};

template <typename T>
struct TS1 {};

template <typename T>
int I3 = 3;

template <typename T>
int I3<TS1<T>> = 31;

template <>
int I3<TS1<S1>> = 311;

template <>
int I3<double> = 32;

int main() {
    (void)I2<int>;
    (void)I3<TS1<int>>;
}
