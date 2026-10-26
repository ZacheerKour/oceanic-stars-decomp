#include "so2.h"
int func_8003B560(void *o, int b) {
    int i, r;

    i = 0;
    while ( 1 ) {
        if ( i >= 7 ) break;
        r = func_80034B6C( i );
        if ( r ) {
            if ( b == func_8003ABD0( o, r, 2 ) ) return 1;
        }
        i++;
    }
    return 0;
}
