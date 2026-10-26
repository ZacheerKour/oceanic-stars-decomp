#include "so2.h"
void func_800364A0(S_80036240 *o) {
    int i;
    S_80036240e *e;
    S_80036240c *c;

    i = 0;
    while ( 1 ) {
        if ( i >= o->wC ) break;
        e = o->p8 + i;
        if ( e->w8 & 1 ) {
            c = e->p0;
            c->p4->fn24( &c->pad0[c->p4->h20] );
        }
        i++;
    }
}
