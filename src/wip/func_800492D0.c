/* 13 bytes differ: frame 40 vs 32 (8 unexplained bytes) and v1=a1 copy; wall class: frame */
#include "so2.h"
void func_800492D0()
{
    int v;
    int i;

    if ( func_80011B88( D_8004EBE4 ) == 0 ) {
        v = D_8004ECB0[0];
        D_8004ECB0[16] = 0;
        if ( v != 0 ) {
            D_8004ECD0[0].h0 = v;
            D_8004EC6C = D_8004EC70[0];
            func_80011654( D_8004EBE4 );
            func_80011804( D_8004EBE4, D_8004EC70[0], 0 );
            for ( i = 1; i < 16; i++ ) {
                D_8004ECB0[i - 1] = D_8004ECB0[i];
                D_8004EC70[i - 1] = D_8004EC70[i];
                if ( D_8004ECB0[i - 1] == 0 ) break;
            }
            D_8004ECB0[15] = 0;
            D_8004EC70[15] = 0;
        }
    }
}
