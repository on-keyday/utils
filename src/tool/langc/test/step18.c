
int *foo;
int bar[10];
int *hoge() {}
int fuga() {}
int checker(int **f);

int main() {
    int x;
    x = 20;
    checker(&foo);
    foo = &x;
    return *foo + bar[5];
}