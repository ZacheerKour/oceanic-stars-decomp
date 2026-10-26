#include "so2.h"
void func_800363A4(S_80036240 *o, int k) {
    int i;
    S_80036240e *e;
    S_80036240c *c;

    i = 0;
    while ( 1 ) {
        if ( i >= o->wC ) break;
        e = o->p8 + i;
        if ( e->w8 & 1 ) {
            if ( k == -1 ) {
                e->w8 |= 2;
                c = e->p0;
                c->p4->fn1C( &c->pad0[c->p4->h18] );
            } else {
                c = e->p0;
                if ( k == c->p4->fn14( &c->pad0[c->p4->h10] ) ) {
                    e->w8 |= 2;
                    c = e->p0;
                    c->p4->fn1C( &c->pad0[c->p4->h18] );
                }
            }
        }
        i++;
    }
}
