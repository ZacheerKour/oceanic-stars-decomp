#include "so2.h"
void func_8003E8C4(int unused, S_8003E8C4 *o, int a2, int a3, int a4, int a5,
                   int a6, int a7, int a8, int a9, int a10, int a11) {
    o->h8 = a2;
    o->hA = a3;
    o->h10 = a4;
    o->h12 = a5;
    o->bC = a6;
    o->bD = a7;
    o->hE = a9;
    o->b4 = a10;
    o->b5 = a10;
    o->b6 = a10;
    o->b7 = a11 ? 102 : 100;
    o->w0 = 0x4000000;
    func_80021D30(&o->w14, 0, 1, a8, 0);
}
