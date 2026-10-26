#include "so2.h"
int func_80036664(S_80036540 *o, int id) {
    S_80036540e *e;
    int t, i, n;

    n = 1;
    t = 0;
    while ( 1 ) {
        if ( t >= 5 ) break;
        i = 0;
        while ( 1 ) {
            if ( i >= o->wC ) break;
            e = o->p8 + i;
            if ( e->w4 == t && ( e->w8 & 1 ) ) {
                if ( e->w0 == id ) return n;
                n++;
            }
            i++;
        }
        t++;
    }
    return 0;
}
