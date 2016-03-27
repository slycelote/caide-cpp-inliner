
namespace ns2 {
    void used() {}
}

using namespace ns2;

namespace ns3 {
    void used3();
}

void f1() {
    using namespace ns3;
    used3();
}

namespace ns4 {
    namespace level2 {
        using namespace ns3;
        namespace level3 {
            void used() {
                used3();
            }
        }
    }
    using namespace ns3;
}

using namespace ns3;

int main() {
    used();
    f1();
    ns4::level2::level3::used();
}

