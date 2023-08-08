i = 0;
while (i < 30) {
    i += 3;
    while (i % 2 != 0) {
        i -= 1;
    }
    if (i % 4 == 0) {
        i += 1;
    }
}
return i;
