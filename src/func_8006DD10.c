#include "so2.h"
void func_8006DD10(int a0)
{
    int i;
    unsigned short f;

    for ( i = 0; i < D_80074BD4; i++ ) {
        f = D_80074BCC[i].h0;
        if ( f & 0x10 ) {
            f = (f & ~0x10) | 2;
            D_80074BCC[i].h0 = f;
            if ( f & 2 ) {
                func_8006DDC4(a0, i + 1);
            }
        }
    }
}
