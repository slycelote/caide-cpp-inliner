template<typename T>
void f1() {}

template<typename T>
void f2() {}

template<>
void f2<int>() {}

int main() {
    f1<double>();
    f2<int>();
}

