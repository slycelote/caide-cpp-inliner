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
        void unused() {}
    }
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
    return 0;
}

