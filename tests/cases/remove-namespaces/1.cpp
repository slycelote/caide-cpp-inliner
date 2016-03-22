namespace ns1 {
    void unused() { }
}

namespace ns1 {
    void used() { }
}

namespace ns2 {
    void used() { }
}

namespace ns2 {
    void unused() {}
}

namespace outer1 {
    namespace inner1 {
        template<typename T>
        struct void_t {};
    }
}

namespace outer1 {
    namespace inner1 {
        template<typename T>
        struct UsedClass {
            template<typename U>
            void UnusedMethod(U u);
        };
    }
}

namespace outer1 {
    namespace inner1 {
        template<typename T>
        template<typename U>
        void UsedClass<T>::UnusedMethod(U u) {}
    }
}

namespace outer1 {
    static int unusedVariable = 1;
}

namespace outer1 {
    template<typename T>
    using UnusedTypeAlias = T;
}

namespace outer2 {
    namespace inner1 {
        void used() {}
    }
    namespace inner2 {
        void unused() {}
    }
}

namespace outer3 {
    namespace inner1 {
        void unused() {}
    }
}

int main() {
    ns1::used();
    ns2::used();
    outer2::inner1::used();
    {
    outer1::inner1::UsedClass<int> x;
    }
    return 0;
}

