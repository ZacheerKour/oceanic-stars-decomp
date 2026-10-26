#include "so2.h"
int func_8006F730(S_8006F1A0 *a0, int a1)
{
    S_8006F1A0p p;
    short r;

    func_8006F218(a0);
    func_80032488(a0->p54, a0->h2C, a0->h2E, 12);
    r = func_80032558(a0->p54, a1);
    p.h0 = 2;
    p.hC = a0->h2C;
    p.hE = a0->h2E;
    p.h2 = 4;
    p.h4 = r;
    func_8006FD10(a0, &p);
    a0->h2E = a0->h2E + a0->h30;
    return a0->h16 - 1;
}
