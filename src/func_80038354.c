#include "so2.h"
unsigned char *func_80038354( unsigned char *src, unsigned char *dst ) {
    unsigned char c, d;
    int n, mode, k, cnt;

    mode = 0;
    n = 0;
    while ( 1 ) {
        c = *src++;
        if ( c == 0 ) break;
        if ( c == 64 ) {
            mode = 1 - mode;
            continue;
        }
        d = *src;
        if ( (unsigned char)( d + 34 ) < 2 ) src++;
        else d = 32;
        k = 0;
        cnt = mode;
        while ( 1 ) {
            if ( c == D_8004B8C8[k * 2] && d == D_8004B8C8[k * 2 + 1] ) {
                if ( cnt == 0 ) break;
                cnt--;
            }
            k++;
        }
        dst[n++] = k + 1;
    }
    dst[n] = 0;
    return dst + n;
}
