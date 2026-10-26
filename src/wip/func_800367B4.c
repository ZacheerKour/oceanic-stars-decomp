/* wall: non-rotated loop + giv (see func_800361CC class), ~70-80 differing bytes */
#include "so2.h"
void func_800367B4(S_80036540 *o) {
    int i, n;
    n = o->wC;
    o->w10 = 0;
    for ( i = 0; i < n; i++ ) {
        if ( ( o->p8[i].w8 & 1 ) && !( o->p8[i].w8 & 0x10 ) ) {
            o->w10 = o->p8[i].w0;
        }
    }
}
