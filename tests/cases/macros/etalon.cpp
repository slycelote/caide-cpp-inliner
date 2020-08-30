#define MACRO_TO_KEEP

#define foo 1
int a() {
    return 2;
}

int b() {
    return 1;
}

int main() {
    return a() + b();
}