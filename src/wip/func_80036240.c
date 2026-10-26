/* wall: register swap only (17-21 bytes): retail f=a0, j=a1 in the two flag-OR loops, ours j=a0, f=a1 (priority class). Tried decl order (24 perms), separate vars, static inline helper, pointer e, operand order, inits order. Semantics: 5 passes of virtual call (slot 0x2C) for entries with w4==t and w8&1, then OR of all w8: &4 -> func_800365A4, &8 -> func_800366FC. */
#include "so2.h"
void func_80036240(S_80036240 *o) {
    int t, i, j, f;
    S_80036240e *e;
    S_80036240c *c;

    t = 0;
    while ( 1 ) {
        if ( t >= 5 ) break;
        i = 0;
        while ( 1 ) {
            if ( i >= o->wC ) break;
            e = o->p8 + i;
            if ( e->w4 == t && ( e->w8 & 1 ) ) {
                c = e->p0;
                c->p4->fn2C( &c->pad0[c->p4->h28] );
            }
            i++;
        }
        t++;
    }
    f = 0;
    j = 0;
    while ( 1 ) {
        if ( j >= o->wC ) break;
        f |= o->p8[j].w8;
        j++;
    }
    if ( f & 4 ) func_800365A4( o );
    f = 0;
    j = 0;
    while ( 1 ) {
        if ( j >= o->wC ) break;
        f |= o->p8[j].w8;
        j++;
    }
    if ( f & 8 ) func_800366FC( o );
}
