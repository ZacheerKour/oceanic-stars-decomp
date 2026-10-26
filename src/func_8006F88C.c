#include "so2.h"
int func_8006F88C(S_8006F1A0 *a0, S_8006F1A0p *a1)
{
    S_8006F88C_r r;
    S_8006F88C_o *o;

    o = func_80067D9C(func_8001FD94(60));
    r.x = 449;
    r.y = 1;
    r.w = 511;
    r.h = 192;
    o->w38 = func_80068E80(&r);
    a1->h0 = 5;
    a1->h2 = 6;
    a1->w8 = (int)o;
    func_8006FD10(a0, a1);
    return a0->h16 - 1;
}
