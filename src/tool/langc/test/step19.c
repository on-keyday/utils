
int main(int argc, char** argv) {
    char x[3];
    x[0] = -1;
    x[1] = 2;
    int y;
    y = 4;
    // return x[0] + y;  // → 3
    return argc + argv[1][0] + x[0] + y;
}