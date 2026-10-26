int func_8003093C(int unused, unsigned mask) {
    int i, bit;
    if (mask) {
        bit = 1;
        for (i = 0; i < 16; i++) {
            if (mask & bit) return bit;
            bit <<= 1;
        }
    }
    return 0;
}
