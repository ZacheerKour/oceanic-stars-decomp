#include "so2.h"
int func_8006FC2C(S_8006F1A0 *a0, S_8006FC2C_blk *a1)
{
    S_8006FC2Cp p;

    p.h0 = 6;
    p.hC = a0->h2C;
    p.hE = a0->h2E;
    p.h2 = 4;
    p.blk14 = *a1;
    p.h4 = 16;
    func_8006FD10(a0, &p);
    return a0->h16 - 1;
}
