#include "so2.h"
void func_8003B7B0(S_8003B7B0 *o, int v) {
    int i, j;

    if ( o->bC24 ) {
        o->bC24 = 0;
        for ( i = 15; i >= 0; i-- ) o->h800[i] = 0;
    }
    for ( i = 0; i < 16; i++ ) {
        if ( v == o->h800[i] ) break;
    }
    if ( i == 16 ) i = 15;
    for ( j = i; j > 0; j-- ) o->h800[j] = o->h800[j - 1];
    o->h800[0] = v;
}
