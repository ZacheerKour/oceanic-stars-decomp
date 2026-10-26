/* wall: tail/schedule, 151 differing bytes: loop shape matches (while(1)+break); retail computes the b2084[slot] address from the already-set a0 and drops the store into the delay slot of the final call, and the two paths merge the other way round (not-found jumps into the found tail) */
#include "so2.h"
int func_8003B638(S_8003B83C *o, int a, int b, int c) {
    int i, slot, n;
    unsigned short *p, e;

    slot = -1;
    i = 0;
    p = o->h0;
    while ( 1 ) {
        if ( i >= 1024 ) break;
        e = *p;
        if ( !( ( e >> 10 ) & 0x1F ) ) {
            if ( slot == -1 ) slot = i;
        } else if ( ( e & 0x3FF ) == a ) break;
        p++;
        i++;
    }
    if ( i == 1024 ) {
        if ( slot < 0 ) return 0;
        o->h0[slot] = a | ( b << 10 ) | ( c << 15 );
        o->b2084[slot] = func_8003B944( o, slot, 0xFFFF );
    } else if ( func_8003B9CC( o, i ) ) {
        n = ( ( o->h0[i] >> 10 ) & 0x1F ) + b;
        if ( n > 20 ) return 0;
        o->h0[i] = a | ( n << 10 ) | ( c << 15 );
        o->b2084[i] = func_8003B944( o, i, 0xFFFF );
    } else {
        o->h0[i] = 0;
        return 1;
    }
    func_8003B7B0( o, a );
    return 1;
}
