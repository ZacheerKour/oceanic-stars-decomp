#include "so2.h"
int func_8006FB68(S_8006F1A0 *a0, short a1)
{
    S_8006F1A0p p;

    p.h0 = 6;
    p.hC = a0->h2C;
    p.hE = a0->h2E;
    p.h4 = a1;
    p.h2 = 4;
    func_8006FD10(a0, &p);
    return a0->h16 - 1;
}
