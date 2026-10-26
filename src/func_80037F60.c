#include "so2.h"
int func_80037F60(S_80037FE0 *o) {
    if ( func_80024AF0(o->wFB0[0]) == 1 ) {
        return 1;
    }
    if ( func_80024AF0(o->wFB0[1]) == 1 ) {
        return 2;
    }
    if ( func_80024AF0(o->wFB0[2]) == 1 ) {
        return 3;
    }
    return ( func_80024AF0(o->wFB0[3]) == 1 ) ? 4 : 0;
}
