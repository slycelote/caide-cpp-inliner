namespace ns1 {
    void used1() {}
}

namespace ns1 {
    void used2() {}
}

namespace ns2 {
    namespace internal {
        void used1() {}
    }
    int unused = 0;
    void unused2(int a) {}
}

namespace ns2 {
    namespace internal {
        void used2() {}
    }
    int used = 1;
}

namespace ns2 {
    namespace internal {
        void used3() {}
    }
}

namespace ns3 {
    void used1() {}
}

struct Unused {
    Unused(const Unused& u) = delete;
};

namespace ns3 {
    void used2() {}
}

int main() {
    ns1::used1();
    ns1::used2();
    ns2::internal::used1();
    ns2::internal::used2();
    ns2::internal::used3();
    (void)ns2::used;
    ns3::used1();
    ns3::used2();
}

