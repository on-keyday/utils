
foo() {
    return 10;
}

bar(a, b, c) {
    return a + b + c;
}

main(argc, argv) {
    x = (1, foo()) +
        (2, bar(2, 4, 3));
    if (x != 19) {
        for (;;) {
        }
    }
    return x + argc - 1;
}