/* 64 differing bytes: wall schedule: retail loads p4 before lhu h2C and the lh h48 before the ori/sh; body and size match (452B) */
#include "so2.h"
void func_80073D4C(S_80073D4C *a0)
{
    S_80073D4Co *o;
    S_80073D4Cq *q;

    a0->h32 = func_80036664(func_8002FFE4()->wC, a0);
    o = a0->pC;
    o->wC = func_8003C144(o->w8);
    if ( a0->b46 != 0 ) {
        if ( a0->h44 != 0xFFFF ) {
            func_80074438(a0, a0->h44);
            a0->h44 = 0xFFFF;
        }
        a0->h2C |= 8;
        a0->p4->fn4C(&a0->pad0[a0->p4->h48]);
        q = a0->pC->wC;
        q->t38.addr = func_8002FFE4()->p4->pC[a0->h32].addr;
        func_8002FFE4()->p4->pC[a0->h32].addr = (unsigned int)a0->pC->wC;
    } else {
        a0->p4->fn4C(&a0->pad0[a0->p4->h48]);
        a0->pC->wC->t38.addr = func_8002FFE4()->p4->pC[a0->h32].addr;
        func_8002FFE4()->p4->pC[a0->h32].addr = (unsigned int)a0->pC->wC;
    }
}
