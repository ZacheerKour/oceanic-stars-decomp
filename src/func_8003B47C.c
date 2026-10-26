#include "so2.h"
int func_8003B47C( void *o, int v ) {
    int f, g;

    if ( v == 0 ) return 0;
    f = 0;
    if ( func_8003B560( o, 165 ) || func_8003B560( o, 220 ) || func_8003B560( o, 221 ) ) f = 1;
    if ( f ) {
        v >>= 1;
    } else {
        g = 0;
        if ( func_8003B560( o, 161 ) || func_8003B560( o, 210 ) ) g = 1;
        if ( g ) v = v * 2 / 3;
    }
    if ( v == 0 ) return 1;
    return v;
}
