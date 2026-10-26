#include "so2.h"
int func_8003B944(S_8003B83C *o, int i, int r) {
    unsigned w, x;
    int hi, lo;
    w = o->h0[i] & 0x7FFF;
    hi = ( ~i & 0xF ) << 4;
    if ( r == 0xFFFF ) {
        r = func_800104D4(16);
    }
    x = w ^ ( w >> 8 );
    lo = ( x & 0xF ) ^ ( ( x & 0xFF ) >> 4 );
    return ( hi ^ ( r << 4 ) | ( lo ^ r ) ) & 0xFF;
}
