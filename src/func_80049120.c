#include "so2.h"
void func_80049120()
{
    int n;
    int i;
    short v;

    n = 0;
    for ( i = 0; i < 16; i++ ) {
        v = D_8004ECB0[i];
        if ( v != 0 ) {
            D_8004ECB0[n] = v;
            D_8004EC70[n] = D_8004EC70[i];
            n++;
        }
    }
    for ( ; n < 16; n++ ) {
        D_8004ECB0[n] = 0;
        D_8004EC70[n] = 0;
    }
}
