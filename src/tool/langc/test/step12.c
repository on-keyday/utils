
unwrap(argv) {
    return **argv;
}

main(argc, argv) {
    x = &argc;
    return *x + unwrap(argv + 8);
}