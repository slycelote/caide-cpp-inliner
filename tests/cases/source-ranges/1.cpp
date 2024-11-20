auto f() -> decltype(1);
auto g() noexcept -> int;

int g() noexcept { return 1; }

int main() { }
