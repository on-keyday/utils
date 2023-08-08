
int main(int argc, int** argv) {
    return sizeof(argc) + sizeof(*(argv + 10));
}