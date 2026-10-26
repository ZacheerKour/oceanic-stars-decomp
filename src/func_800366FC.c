#include "so2.h"
void func_800366FC(S_80036540 *o) {
    int i, n;
    S_80036540e *old, *e;

    if ( o->wC ) {
        n = o->wC;
        old = o->p8;
        i = 0;
        o->wC = 0;
        o->w14 = 0;
        o->p8 = 0;
        e = old;
        while ( 1 ) {
            if ( i >= n ) break;
            if ( e->w8 & 1 ) {
                if ( !( e->w8 & 8 ) ) func_80036818( o, e );
            }
            e++;
            i++;
        }
        func_8001FD70( old );
        func_800367B4( o );
    }
}
