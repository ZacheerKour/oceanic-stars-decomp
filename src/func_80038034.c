#include "so2.h"
int func_80038034(S_80037FE0 *o) {
    if ( func_80024AF0(o->wFB0[4]) == 1 ) {
        return 1;
    }
    if ( func_80024AF0(o->wFB0[5]) == 1 ) {
        return 2;
    }
    if ( func_80024AF0(o->wFB0[6]) == 1 ) {
        return 3;
    }
    return ( func_80024AF0(o->wFB0[7]) == 1 ) ? 3 : 0;
}
