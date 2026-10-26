/* 12 differing bytes: wall register/schedule: retail loads h2E,h30,h16 (lhu 46,48,22) before the addu; ours h2E,h16,h30. Permute candidate. */
#include "so2.h"
int func_8006F1A0(S_8006F1A0 *a0, int a1)
{
    S_8006F1A0p p;

    p.h0 = 1;
    p.hC = a0->h2C;
    p.hE = a0->h2E;
    p.h12 = a0->h30;
    p.w8 = a1;
    p.h2 = 8;
    func_8006FD10(a0, &p);
    a0->h2E = a0->h2E + a0->h30;
    return a0->h16 - 1;
}
