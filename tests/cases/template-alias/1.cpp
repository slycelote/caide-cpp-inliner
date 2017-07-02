template<typename T>
struct Type {
    using type = int;
};

template<typename T>
using Alias = typename Type<T>::type;

int main() {
    Alias<void> a;
    return 0;
}
