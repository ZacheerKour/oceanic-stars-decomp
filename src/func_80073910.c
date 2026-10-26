#include "so2.h"
void func_80073910(int a0)
{
    S_80073910s *s;
    S_80073910e *e;
    int i;

    s = (S_80073910s *)func_8002FFE4()->wC;
    i = 0;
    while ( 1 ) {
        if ( i >= s->wC ) break;
        e = s->p8 + i;
        if ( e->w8 & 1 ) {
            if ( e->p0->p4->fn14(&e->p0->pad0[e->p0->p4->h10]) & 0x10 ) {
                if ( e->p0->w8 != 0 && e->p0->w8 == a0 ) {
                    e->p0->p4->fn24(&e->p0->pad0[e->p0->p4->h20]);
                    func_80023F40(D_80074AC0);
                }
            }
        }
        i++;
    }
    func_80036540(func_8002FFE4()->wC, a0);
}
