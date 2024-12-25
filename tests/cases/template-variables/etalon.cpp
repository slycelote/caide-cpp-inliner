template <typename T>
int I2 = 2;

template <typename T>
struct TS1 {};

template <typename T>
int I3 = 3;

template <typename T>
int I3<TS1<T>> = 31;

int main() {
    (void)I2<int>;
    (void)I3<TS1<int>>;
}
