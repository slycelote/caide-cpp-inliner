void f1() {}
void f2() {
    f1();
}

int gen() {
    static int c = 0;
    return ++c;
}

void f4() {
    struct S1 {
    };
    S1 x;
    static int i = gen();
}


/*
TODO
template<typename T>
struct Outer
{
    struct Inner
    {
    };

    Inner inner;
};

template<typename T>
void f3() {
    Outer<T> v;
    v.inner;
}
*/

int f6() {
    return 6;
}

struct S2 {
    int i;
    S2()
        : i(f6())
    {}
};

int f7() {
    return 7;
}

void f8(int i = f7()) {
}

/// caide keep
void f9() {}

int i4 ;

int i8, i10;

#define td int

td v2;

void usedFunc() {}

typedef double db;
db dp[100];

struct A {
};
typedef A VI;
struct B : VI {
};

typedef double DD;

typedef A atd1;
typedef A atd2;
typedef A atd3;

int f(atd3& a){}

int main() {
    f2();
    //f3<int>();
    f4();
    S2 s2;
    f8();
    i4 = i8 = i10 = 1;
    usedFunc();
    v2 = 1;
    dp[0] = 1;
    B b;
    dp[0] = (DD)1;
    atd1* ptr = 0;
    new atd2[10];
    f(b);
}

