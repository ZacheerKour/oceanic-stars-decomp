/* wall: non-rotated loop + giv (see func_800361CC class), ~70-80 differing bytes */
#include "so2.h"
int func_80036540(S_80036540 *o, int id) {
    int i;
    for ( i = 0; i < o->wC; i++ ) {
        if ( o->p8[i].w0 == id ) {
            o->p8[i].w8 = ( o->p8[i].w8 & ~1 ) | 4;
            return 1;
        }
    }
    return 0;
}
