struct S1 { using type = int; };

template<class T, class U = typename T::type>
void f1() { }

struct S2 {
    using type = int;
    using type2 = int;
};

template<class T1, class T2=typename T1::something, class U = typename T2::type>
void f2(T2, typename T2::type2 x) { }

template <typename T>
struct S3 {
    using type = T;
};

template <class T, class U = typename T::type>
void f3(T&) { }

int main() {
    f1<S1>();
    f2<double>(S2{}, 0);

    S3<int> s3;
    f3(s3);
}
