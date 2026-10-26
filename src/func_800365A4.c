#include "so2.h"
void func_800365A4(S_80036240 *o) {
    int i;
    S_80036240e *e;
    S_80036240c *c;

    i = 0;
    while ( 1 ) {
        if ( i >= o->wC ) break;
        e = o->p8 + i;
        if ( e->w8 & 4 ) {
            e->w8 = ( e->w8 & ~4 ) | 8;
            c = e->p0;
            if ( c ) c->p4->fn0C( &c->pad0[c->p4->h8], 3 );
        }
        i++;
    }
}
