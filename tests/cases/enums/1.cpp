using Int1 = int;
enum UnusedEnum { a, b, c};
enum class UnusedEnumClass { d, e, f};

using Int2 = int;
constexpr int f() { return 10; }
enum UsedEnum { g, h = f(), i};
enum class UsedEnumClass: Int2 { j, k, l};

int main() {
    (void)g;
    (void)UsedEnumClass::j;
}

