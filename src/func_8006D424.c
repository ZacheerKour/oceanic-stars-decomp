#include "so2.h"
int func_8006D424(int a0)
{
    int i;

    for ( i = 0; i < D_80074BD4; i++ ) {
        if ( D_80074BCC[i].h0 & 2 ) {
            func_8006DDC4(a0, i + 1);
        }
    }
    return a0;
}
