#include "so2.h"
void func_80048F7C( int a, int b, int idx )
{
    int i;
    int j;

    if ( idx == -1 ) {
        for ( i = 0; i < 16; i++ ) {
            if ( D_8004ECB0[i] == 0 ) {
                idx = i;
                break;
            }
        }
    }
    if ( idx != -1 ) {
        if ( D_8004ECB0[idx] != 0 ) {
            for ( j = 15; idx < j; j-- ) {
                D_8004ECB0[j] = D_8004ECB0[j - 1];
                D_8004EC70[j] = D_8004EC70[j - 1];
            }
        }
        D_8004ECB0[idx] = a;
        D_8004EC70[idx] = b;
    }
}
