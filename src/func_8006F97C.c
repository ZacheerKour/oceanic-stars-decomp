#include "so2.h"
unsigned short func_8006F97C(S_8006F1A0 *a0, int a1, int a2, int a3)
{
    S_800340C0 *e;
    int i;
    int b;

    e = D_8004DDA8;
    for ( i = 0; i < 8; i++, e++ ) {
        if ( e->head.h0 == a1 ) {
            b = e->head.b2;
        }
    }
    return func_8006F930(a0, a1, b, a2, a3);
}
