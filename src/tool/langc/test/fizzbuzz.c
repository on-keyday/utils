
int printf(const char* fmt, ...);
int atoi(const char* str);
void exit(int status);
int foo();

void print(const char* ptr) {
    printf("%s", ptr);
}

int strlen(const char* s) {
    const char* start;
    start = s;
    while (*s) {
        s++;
    }
    print("reach");
    return s - start;
}

int main(int argc, char** argv) {
    foo();
    if (argc < 2) {
        printf("need 1 argument");
        exit(1);
    }
    int len;
    len = strlen(argv[1]);
    int n;
    n = atoi(argv[1]);
    int i;
    i = 1;
    for (; i <= n; i++) {
        if (i % 15 == 0) {
            print("FizzBuzz");
        }
        else if (i % 3 == 0) {
            print("Fizz");
        }
        else if (i % 5 == 0) {
            print("Buzz");
        }
        else {
            printf("%d", i);
        }
        print("\n");
    }
    print("end\n");
    return len;
}