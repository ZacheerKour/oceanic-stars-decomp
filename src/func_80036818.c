#include "so2.h"
void func_80036818(S_80036540 *o, S_80036540e *e) {
    S_80036540e *old;
    if ( o->w14 <= o->wC ) {
        old = o->p8;
        o->w14 += 20;
        o->p8 = func_8001FE30(o->w14 * 12);
        func_80023ED0(o->p8, old, o->wC * 12);
        func_8001FD70(old);
    }
    o->p8[o->wC] = *e;
    o->wC++;
}
