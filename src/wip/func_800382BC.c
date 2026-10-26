/* wall: non-rotated (top-tested) loop, exit block at function end; 108-111 bytes differ. Retail hoists consts above the loop and jumps back to the lbu; ours rotates (duplicates lbu before loop). func_80038354 (same family) matched with unsigned char c (andi before the test blocks rotation) and dst[n++] (giv init move lands after hoisted consts); here c is a plain int so the test is a bare lbu+beqz and gcc rotates. Tried: dst[n++] vs out++, while(*src), c=*src++, goto, do-while, continue, return-in-loop (unrotated but exit inline), decl order (120 perms). Semantics: text -> pair table D_8004B8C8, 64 = shift marker, 32 = skip pad.  Wave 10 also tried: unsigned char c with c=*src++;if(!c)break;c-=1 (while/for): still rotated, src copied to t0 (109+ bytes). */
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
