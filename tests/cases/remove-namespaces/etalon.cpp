namespace ns1 {
    void used() { }
}

namespace ns2 {
    void used() { }
}


namespace outer1 {
    namespace inner1 {
        template<typename T>
        struct UsedClass {
        };
    }
}

namespace outer2 {
    namespace inner1 {
        void used() {}
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


