/* 48 differing bytes: wall schedule + retail keeps a dead lhu h16/addiu -1 after the call (not reproducible by an inlined return value) */
#include "so2.h"
void func_8006F7CC(S_8006F1A0 *a0, short a1)
{
    S_8006E80Ce e;
    unsigned short s;

    func_8006F218(a0);
    func_80032488(a0->p54, a0->h2C, a0->h2E, 12);
    s = func_800325B4(a0->p54, a1, 0);
    func_80032840(a0->p54, s);
    func_800329B0(a0->p54);
    e.h0 = 2;
    e.hC = a0->h2C;
    e.h2 = 4;
    e.h4 = s;
    e.hE = a0->h2E;
    func_8006FD10(a0, &e);
    a0->h2E += a0->h30;
}
