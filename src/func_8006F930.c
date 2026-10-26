#include "so2.h"
unsigned short func_8006F930(S_8006F1A0 *a0, short a1, short a2, short a3, int a4)
{
    S_8006F930p p;

    p.hC = a0->h2C + 16;
    p.hE = a0->h2E + 42;
    p.h10 = a1;
    p.h12 = a3;
    p.h14 = a2;
    p.h4 = a4;
    return func_8006F88C(a0, &p);
}
