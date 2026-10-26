/* wall: non-rotated (top-tested) loop; retail hoists table consts above the loop and jumps back to the lbu at the bottom (same class as 800361CC/800386FC). Tried while(c=*src++), for(;;)+break, test-before-increment: all rotate. Semantics are right (text -> pair table D_8004B8C8, 64 = shift marker, 32 = skip pad), 109 bytes differ */
#include "so2.h"
unsigned char *func_800382BC( unsigned char *src, unsigned char *dst ) {
    unsigned char *out;
    int n, mode, m;
    unsigned int c;
    unsigned char hi, lo;

    n = 0;
    mode = 0;
    out = dst;
    for ( ;; ) {
        if ( !*src ) break;
        c = *src;
        src++;
        c = ( c - 1 ) & 0xFF;
        m = c >= 140;
        hi = D_8004B8C8[c * 2];
        lo = D_8004B8C8[c * 2 + 1];
        if ( mode != m ) {
            *out++ = 64;
            n++;
        }
        *out++ = hi;
        n++;
        if ( lo != 32 ) {
            *out++ = lo;
            n++;
        }
        mode = m;
    }
    dst[n] = 0;
    return dst + n;
}
