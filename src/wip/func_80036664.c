/* wall: non-rotated loop + giv (see func_800361CC class), ~70-80 differing bytes */
#include "so2.h"
int func_80036664(S_80036540 *o, int id) {
    int t, i, n;
    n = 1;
    for ( t = 0; t < 5; t++ ) {
        for ( i = 0; i < o->wC; i++ ) {
            if ( o->p8[i].w4 == t && ( o->p8[i].w8 & 1 ) ) {
                if ( o->p8[i].w0 == id ) {
                    return n;
                }
                n++;
            }
        }
    }
    return 0;
}
