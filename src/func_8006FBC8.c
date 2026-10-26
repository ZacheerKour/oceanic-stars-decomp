#include "so2.h"
int func_8006FBC8(S_8006F1A0 *a0, short a1, int a2, short a3)
{
    S_8006F1A0p p;

    p.h0 = 7;
    p.hC = a0->h2C;
    p.w14 = a2;
    p.hE = a0->h2E;
    p.h1A = a1;
    p.h2 = 4;
    p.h18 = a3;
    func_8006FD10(a0, &p);
    return a0->h16 - 1;
}
