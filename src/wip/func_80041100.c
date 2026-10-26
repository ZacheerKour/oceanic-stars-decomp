/* 124 differing bytes: wall control-flow: retail outer loop (i<128) is top-tested, not rotated; also tail uses a copy of the base pointer (move v0,s0) */
#include "so2.h"
void func_80041100(S_80041100 *a0)
{
    int i;
    int j;
    S_80041100r *r;

    r = a0->r;
    i = 0;
    while ( i < 128 ) {
        for ( j = 0; j < 34; j++ ) {
            r->w[j] = 0;
        }
        r++;
        i++;
    }
    for ( j = 0; j < 24; j++ ) {
        a0->w4400[j] = 0;
    }
    a0->p447C = (S_80042BB0 *)func_8001FD94(16);
    func_8003C57C(a0->p447C, 0x1C000);
    func_8003C5B4(a0->p447C, 128);
    a0->w4460 = 1;
    a0->w4464 = 0;
}
