#include "so2.h"
void func_8006D4C8(int a0, int a1)
{
    int i;

    for ( i = 0; i < D_80074BD4; i++ ) {
        if ( D_80074BCC[i].h0 & 2 ) {
            func_8006DDC4(a0, i + 1);
        }
    }
    if ( a1 & 1 ) {
        func_8001FD70(a0);
    }
}
