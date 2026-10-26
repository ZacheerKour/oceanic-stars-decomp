#include "so2.h"
int func_8006FA7C(S_8006F1A0 *a0, S_8006FA7C *a1, int a2, int a3)
{
    S_8006E80Ce e;
    S_8006FA7C *c;

    c = func_8001FD94(40);
    *c = *a1;
    e.h0 = 4;
    e.hC = a0->h2C;
    e.hE = a0->h2E;
    e.h10 = a2;
    e.h12 = a3;
    e.h4 = 0;
    e.h2 = 6;
    e.p8 = c;
    func_8006FD10(a0, &e);
    a0->h2E += a0->h30;
    return a0->h16 - 1;
}
