#include "so2.h"
void func_8006F3C8(S_8006F1A0 *a0, short a1)
{
    int x;
    int y;
    short a;
    int b;
    int c;
    int h2C;
    int h2E;

    h2C = a0->h2C;
    h2E = a0->h2E;
    func_80032308(a0->p54, &x, &y);
    a = func_80032374(a0->p54);
    b = func_80034440(1);
    c = func_80034440(0);
    func_8006F2B0(a0, b, c, 3);
    func_8006F4E8(a0, a1);
    func_8006F26C(a0, 0);
    func_80032488(a0->p54, h2C + 106, h2E, 12);
    func_80032524(a0->p54, 15);
    func_80032488(a0->p54, h2C + 110, h2E, 12);
    func_800324A0(a0->p54, func_800344D0(a1), 2);
    func_8006F2B0(a0, x, y, a);
}
