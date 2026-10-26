/* wip: needs D_8004DDC4->hC (halfword at 0xC), but S_80030CBC (lane 1 retype) has a pointer p8[1] there; two views of one address, unresolved. Matched in isolation under the old S_80030C00b type. */
#include "so2.h"
void func_80074008(S_80074008 *o)
{
    int a, b, d, t;
    int s1;
    S_80074008_n *n;

    D_8004DDC4->hC = o->h42;
    if ( o->h2C & 0xa ) {
        return;
    }
    o->w24 = func_8002FFE4()->w20;
    o->w28 = func_8002FFE4()->w24;
    if ( o->h38 ) {
        o->h38--;
        return;
    }
    d = func_8002FFE4()->w1C;
    b = o->w24;
    n = o->p14;
    a = o->w28;
    if ( !n ) {
        return;
    }
    if ( n->h30 & 2 ) {
        return;
    }
    t = n->h34;
    s1 = (short)( t + 1 );
    if ( func_8006935C( n, 2048 ) ) {
        o->p4->fn64( &o->pad0[o->p4->h60], s1 );
    }
    if ( a & D_8004EBFC->h0 ) {
        o->h36 = 7;
        o->p4->fn6C( &o->pad0[o->p4->h68], s1 );
    } else if ( a & D_8004EBFC->h2 ) {
        o->h36 = 7;
        o->p4->fn74( &o->pad0[o->p4->h70], s1 );
    } else if ( a & D_8004EBFC->h4 ) {
        o->h36 = 7;
        o->p4->fn7C( &o->pad0[o->p4->h78], s1 );
    } else if ( a & D_8004EBFC->h6 ) {
        o->h36 = 7;
        o->p4->fn84( &o->pad0[o->p4->h80], s1 );
    } else {
        if ( d ) {
            o->p4->fn8C( &o->pad0[o->p4->h88], d );
        }
        if ( a ) {
            o->p4->fn94( &o->pad0[o->p4->h90], a );
        }
        if ( b & 0x8000 ) {
            o->h38 = 4;
            o->h36 = 7;
            o->p4->fn9C( &o->pad0[o->p4->h98], 1 );
        } else if ( b & 0x2000 ) {
            o->h38 = 4;
            o->h36 = 7;
            o->p4->fn9C( &o->pad0[o->p4->h98], 2 );
        } else if ( b & D_8004EBFC->hC ) {
            o->h36 = 7;
            o->h38 = 4;
            o->p4->fnA4( &o->pad0[o->p4->hA0], 1 );
        } else if ( b & D_8004EBFC->hE ) {
            o->h36 = 7;
            o->h38 = 4;
            o->p4->fnA4( &o->pad0[o->p4->hA0], 2 );
        }
    }
}
