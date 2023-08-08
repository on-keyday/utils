
int foo() {
    return 10;
}

int bar(int a, int b, int c) {
    return a + b + c;
}

int hoge(int* p) {
    *(p + 2) = 100;
    return 0;
}

int checker(int** ch) {
    *ch = (int*)0x3f3f3f3f3f3f3f3f;
    return 0;
}
#include <stdarg.h>
#include <stdio.h>

int print(const char* s, ...) {
    float v = 0;
    va_list list;
    va_start(list, s);
    vprintf(s, list);
    va_end(list);
    return 0;
}