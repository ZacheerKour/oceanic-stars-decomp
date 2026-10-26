#include "so2.h"
int func_8006F0D0(S_8006F1A0 *a0, S_8006F0D0_pt *a1, char *a2)
{
    S_8006F1A0p p;
    void *s;

    s = func_8001FE30(func_80035BC8(a2) + 1);
    func_80023ED0(s, a2, func_80035BC8(a2) + 1);
    p.h0 = 1;
    p.hC = a1->h0 + a0->h2C;
    p.hE = a1->h2 + a0->h2E;
    p.h12 = a0->h30;
    p.h2 = 6;
    p.w8 = (int)s;
    func_8006FD10(a0, &p);
    a0->h2E = a0->h2E + a0->h30;
    return a0->h16 - 1;
}
