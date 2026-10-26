#include "so2.h"
void func_80048C40(S_80048D2C *a0)
{
    int c;

    func_8004A438(a0);
    func_8004A3D0(a0, 4, 0, ( a0->w20 & 3 ) | 200, 4);
    c = a0->w20 >= 4 ? 206 : 204;
    func_8004A3D0(a0, 4, 14, c, 0);
    func_8004A3D0(a0, 4, 28, c | 1, 0);
    func_8004A500(a0);
    func_8004950C(a0);
}
