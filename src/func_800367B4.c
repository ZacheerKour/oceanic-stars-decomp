#include "so2.h"
void func_800367B4(S_80036540 *o) {
    S_80036540e *e;
    int i, n;

    n = o->wC;
    o->w10 = 0;
    i = 0;
    while ( 1 ) {
        if ( i >= n ) break;
        e = o->p8 + i;
        if ( e->w8 & 1 ) {
            if ( !( e->w8 & 0x10 ) ) o->w10 = e->w0;
        }
        i++;
    }
}
