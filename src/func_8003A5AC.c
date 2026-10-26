#include "so2.h"
void func_8003A5AC(void *o) {
    int i, a, v, r;

    i = 0;
    while ( 1 ) {
        if ( i >= 8 ) break;
        func_800341F0( i + 259, func_80034110( i + 3 ) );
        i++;
    }
    a = func_80034110( 1 );
    func_800341F0( 257, a + a * D_8004EBFC->h184 / 100 );
    a = func_80034110( 2 );
    func_800341F0( 258, a + a * D_8004EBFC->h186 / 100 );
    func_8003AD68( o );
    i = 0;
    while ( 1 ) {
        if ( i >= 8 ) break;
        func_800341F0( i + 515, func_80034110( i + 259 ) );
        i++;
    }
    if ( D_8004DDB8 ) func_8003A738( o );
    i = 0;
    while ( 1 ) {
        if ( i >= 2 ) break;
        v = func_80034110( i + 257 );
        r = func_80034110( i + 513 );
        if ( v < r ) func_800341F0( i + 513, v );
        i++;
    }
}
