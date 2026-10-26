#include "so2.h"
unsigned char *func_800382BC( unsigned char *src, unsigned char *dst ) {
    int n, mode, m;
    unsigned int c;
    unsigned char hi, lo;
    mode = 0;
    n = 0;
    while ( 1 ) {
        c = *src;
        if ( c == 0 ) break;
        src++;
        c = ( c - 1 ) & 0xFF;
        m = c >= 140;
        hi = D_8004B8C8[c * 2];
        lo = D_8004B8C8[c * 2 + 1];
        if ( mode != m ) dst[n++] = 64;
        dst[n++] = hi;
        if ( lo != 32 ) dst[n++] = lo;
        mode = m;
        continue;
    }
    dst[n] = 0;
    return dst + n;
}
