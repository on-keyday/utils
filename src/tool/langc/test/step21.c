
int print(const char* fmt, ...);
int printf(const char* fmt, ...);

int main(int argc, char** argv) {
    const char* name;
    name = argv[1];
    print("Hello %s or %s%s%s%s%s\n", name, name, name, name, name, name);
    printf("Hello %s\n", name);
    printf("Hello %s", "World\n");
    return 0;
}
