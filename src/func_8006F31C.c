#include "so2.h"
void func_8006F31C(S_8006F1A0 *a0, short a1)
{
    int x;
    int y;
    short v;
    int r1;
    int r0;

    func_80032308(a0->p54, &x, &y);
    v = func_80032374(a0->p54);
    r1 = func_80034440(1);
    r0 = func_80034440(0);
    func_8006F2B0(a0, r1, r0, 3);
    func_8006F4E8(a0, a1);
    func_8006F2B0(a0, x, y, v);
}
