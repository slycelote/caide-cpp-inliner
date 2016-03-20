namespace ns1 {
    void unused() {}
}

namespace ns2 {
    void used() {}
}

using namespace ns1;
using namespace ns2;

int main() {
    used();
}
