#include "so2.h"
int func_8006F9D8(S_8006F1A0 *a0, int a1, int a2, short a3)
{
    S_8006F930p p;

    p.hC = a0->h2C + 16;
    p.hE = a0->h2E + 4;
    p.h10 = 0;
    p.h12 = a3;
    p.h4 = 4096;
    func_8006F88C(a0, &p);
    return func_8006F97C(a0, a1, a2, 4096);
}
