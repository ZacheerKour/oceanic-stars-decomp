#include "so2.h"
void func_8006E140(int a0, short a1, short a2, short a3, int a4)
{
    S_8006E20C s;
    S_8006E1A0 r;

    r.x = a1;
    r.y = a2;
    r.w = a3;
    r.h = a4;
    s.h0 = 2;
    s.r1 = r;
    s.w14 = a0;
    func_8006E20C(&s);
}
