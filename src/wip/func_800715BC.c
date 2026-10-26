/* 37 differing bytes: wall schedule: retail keeps the o/i loads after the w0/w4 stores (alias) and loads the w10 word before lui; body otherwise matches */
#include "so2.h"
void func_800715BC(S_80072FBC *a0, S_800715BC *a1)
{
    S_800715BCp *p;
    int lo, hi, d;
    S_8003EC54_ot *o;
    int i;
    unsigned short t;

    p = func_8003C434(a0->p58->p4, 40);
    *p = *a1->p8;
    if ( a1->h10 != 0 ) {
        t = a1->h4;
        lo = t & 0xFF;
        hi = t >> 8;
        hi++;
        if ( hi >= a1->h12 ) {
            hi = 0;
            lo++;
            if ( lo >= a1->h10 ) {
                lo = 0;
            }
        }
        a1->h4 = (hi << 8) | lo;
        d = lo * (p->b1D - p->bD);
        p->bD += d;
        p->b15 += d;
        p->b1D += d;
        p->b25 += d;
    }
    p->h8 += a0->h6A + a1->hC;
    p->hA += a0->h6C + a1->hE;
    p->h10 += a0->h6A + a1->hC;
    p->h12 += a0->h6C + a1->hE;
    p->h18 += a0->h6A + a1->hC;
    p->h1A += a0->h6C + a1->hE;
    p->h20 += a0->h6A + a1->hC;
    p->h22 += a0->h6C + a1->hE;
    p->u.w0 = 0x09000000;
    p->w4 = a0->w10 | 0x2C000000;
    o = a0->p58;
    i = a0->h60;
    p->u.tag.addr = o->p0C[i].addr;
    o->p0C[i].addr = (unsigned int)p;
}
