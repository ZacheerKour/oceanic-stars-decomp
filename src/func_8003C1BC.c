#include "so2.h"
void func_8003C1BC( int idx ) {
    int *p, *q, i;
    unsigned int next;

    p = D_8004E0D8[idx][D_8004DED4];
    q = p;
    next = (unsigned int)( p + 1 ) & 0xFFFFFF;
    for ( i = 0; i < D_8004E138[idx] - 1; i++ ) {
        if ( *p == next ) *q = next;
        else q = p + 1;
        next += 4;
        p++;
    }
}
