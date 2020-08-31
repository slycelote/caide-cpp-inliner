#define MACRO_TO_KEEP

#define foo 1
int a() {
#if 123 < foo
    return 1;
#else
    return 2;
#endif
}

int b() {
#if __cplusplus > 1
    return 1;
#else
    return 2;
#endif
}

#define bar 1
int c() {
#if 1 + 1 > 1*(bar+bar)
    return 1;
#else
    return 2;
#endif
}

int main() {
    return a() + b() + c();
}

#define UNUSED 1
int unused_func() {
    return UNUSED;
}
#undef UNUSED

