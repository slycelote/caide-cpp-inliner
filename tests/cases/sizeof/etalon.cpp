int usedVar;
struct usedType { };

int main() {
    int a = sizeof(usedVar), b = sizeof(usedType);
    (void)a; (void)b;
}

