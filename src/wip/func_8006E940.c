/* 69 differing bytes: wall register: body matches; retail keeps o in v1/d in a0 and the clamp result in the a2 register (s2) with a separate copy s4 */
#include "so2.h"
int func_8006E940(S_8006E940 *a0, short a1, short a2, short a3, unsigned short a4, unsigned short a5, unsigned short a6)
{
    S_8006E940o *o;
    short s;
    int d;
    int i;

    if ( a1 == 0 ) {
        a1 = 1;
    }
    o = func_8001FD94(38);
    o->h0 = 0;
    d = a1 - a2;
    if ( d < 0 ) {
        o->h4 = 0;
        s = a1;
    } else {
        o->h4 = d;
        s = a2;
    }
    o->h2 = a1;
    o->h6 = a2;
    o->h8 = a2 + 1;
    o->hA = a3;
    o->hC = a4;
    o->hE = a5;
    o->h10 = a6;
    a0->p5C = o;
    func_8006F218(a0);
    for ( i = 0; i < a0->p5C->h8; i++ ) {
        func_80032488(a0->p54, 0, 0, 12);
        a0->p5C->h12[i] = func_800325B4(a0->p54, a3, 1);
        func_80032840(a0->p54, a0->p5C->h12[i]);
        func_800329B0(a0->p54);
    }
    return s;
}
