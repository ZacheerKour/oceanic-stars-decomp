/* 39 differing bytes: wall schedule: retail loads the 3 stack args before addu/sw w8; same class as 80073204 */
#include "so2.h"
S_800732C0p *func_800732C0(S_800732C0 *a0, S_800732C0p *a1, int a2, int a3, int a4, int a5, int a6, int a7)
{
    S_80073204o *o;
    int i;
    int c;

    c = a7;
    a1->u.w0 = 0x04000000;
    a1->w10 = a4 + (a5 << 16);
    a1->w8 = a2 + (a3 << 16);
    a1->w4 = func_80038430(a0, 0x50000000, a0->w10, a6);
    a1->wC = func_8003853C(a0, a0->w10, c);
    o = a0->p58;
    i = a0->h60;
    a1->u.tag.addr = o->pC[i].addr;
    o->pC[i].addr = (unsigned int)a1;
    return a1 + 1;
}
