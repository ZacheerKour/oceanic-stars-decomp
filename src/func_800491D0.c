#include "so2.h"
int func_800491D0(int a0)
{
    int i;

    if ( a0 == 0 ) {
        if ( D_8004ECD0[0].h0 != 0 ) {
            return 0;
        }
        return D_8004ECB0[0] == 0;
    }
    if ( D_8004ECD0[0].h0 == a0 ) {
        return 0;
    }
    for ( i = 0; i < 16; i++ ) {
        if ( D_8004ECB0[i] == a0 ) {
            return 0;
        }
    }
    return 1;
}
