/* 28 differing bytes: wall register: retail keeps e in a1 (reused arg reg), ours a0; bit-pack ops match */
#include "so2.h"
void func_8006D77C(int a0, short a1, short a2)
{
    short i;
    S_8006D588 *e;
    unsigned short h4;

    i = func_8006D8A4(a0, (a1 + 3) / 4, a2);
    e = D_80074BCC + i - 1;
    e->h0 |= 4;
    h4 = e->h4;
    e->ha = ((h4 >> 4) & 0x10) | ((e->h2 & 0x3FF) >> 6) | ((h4 & 0x200) << 2);
}
