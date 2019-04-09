namespace case1 {

    class Class {};
    template<typename T> class ClassTemplate {};
    template<typename T> class AnotherClassTemplate {};
    void func() {};
    template<typename T> void funcTemplate() {};
    template<typename T> void anotherFuncTemplate() {};

    struct A {
        friend class Class;
        template<typename T> friend class ClassTemplate;
        friend class AnotherClassTemplate<int>;
        friend void func();
        template<typename T> friend void funcTemplate();
        friend void anotherFuncTemplate<int>();
    };
    void main() { A a; }

}


int main() {
    case1::main();
}

