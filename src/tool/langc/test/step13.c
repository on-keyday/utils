
int main(int argc, int** argv) {
    int x;
    x = 0;
    return argc + **(argv + 1);
}