int bar(int a, int b, int c);
int foo() {
    return 10;
}

int val(int* ptr, int val) {
    *ptr = val;
    return 60;
}

int hoge(int* p);

int main(int argc, int** argv) {
    int ptr;
    ptr += val(&ptr, 30) + ptr;
    ptr += foo();
    ptr += bar(1, 2, 3);

    int val;
    // hoge(&val);
    int x;
    int* y;
    y = &x;
    *y = 3;
    *y += 2;
    // return x;
    return ptr - 31 - x;
}