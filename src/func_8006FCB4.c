#include "so2.h"
int func_8006FCB4(S_8006F1A0 *a0, int a1, short a2)
{
    S_8006F1A0p p;

    p.h0 = 8;
    p.w8 = a1;
    p.hC = a2;
    p.h2 = 4;
    p.h4 = func_80073418(a0);
    func_8006FD10(a0, &p);
    return a0->h16 - 1;
}
