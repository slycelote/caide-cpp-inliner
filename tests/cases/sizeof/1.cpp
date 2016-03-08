int usedVar;
struct usedType { };
int unusedVar;
struct unusedType { };

int main() {
    int a = sizeof(usedVar), b = sizeof(usedType);
    (void)a; (void)b;
}

