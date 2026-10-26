#include "so2.h"
int func_800380B8(S_800380B8 *o) {
    while ( 1 ) {
        if ( func_80024AF0( o->wFC0 ) == 1 ) return 1;
        if ( func_80024AF0( o->wFC4 ) == 1 ) return 2;
        if ( func_80024AF0( o->wFC8 ) == 1 ) return 3;
        if ( func_80024AF0( o->wFCC ) == 1 ) return 4;
    }
}
