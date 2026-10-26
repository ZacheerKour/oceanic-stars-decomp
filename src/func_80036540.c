#include "so2.h"
int func_80036540(S_80036540 *o, int id) {
    S_80036540e *e;
    int i;

    i = 0;
    while ( 1 ) {
        if ( i >= o->wC ) break;
        e = o->p8 + i;
        if ( e->w0 == id ) {
            e->w8 = ( e->w8 & ~1 ) | 4;
            return 1;
        }
        i++;
    }
    return 0;
}
