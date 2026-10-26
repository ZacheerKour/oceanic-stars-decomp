#include "so2.h"
void func_80049060( int a0, int a1 )
{
    int i;

    if ( D_8004ECB0[16] == a0 ) {
        if ( a1 == 0 || a1 == D_8004EC6C ) {
            func_80011B48( D_8004EBE4 );
            D_8004ECD0[0].h0 = 0;
            return;
        }
    }
    for ( i = 0; i < 16; i++ ) {
        if ( D_8004ECB0[i] == a0 ) {
            if ( a1 == 0 || a1 == D_8004EC70[i] ) {
                D_8004ECB0[i] = 0;
                func_80049120();
                return;
            }
        }
    }
}
