#include "so2.h"
int func_8003B83C(S_8003B83C *o, int i, int k) {
    int d, r;
    d = ( ( o->h0[i] >> 10 ) & 0x1F ) - k;
    if ( d < 0 ) {
        return 0;
    }
    if ( d == 0 ) {
        o->h0[i] = 0;
    } else {
        o->h0[i] = ( o->h0[i] & 0x83FF ) | ( d << 10 );
    }
    r = func_8003B944(o, i, 0xFFFF);
    o->b2084[i] = r;
    return r;
}
