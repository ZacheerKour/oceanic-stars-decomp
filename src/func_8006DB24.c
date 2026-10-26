#include "so2.h"
int func_8006DB24(int a0, S_8006D588 *a1)
{
    int i;
    S_8006D588 *old;

    a1->h0 |= 1;
    for ( i = 0; i < D_80074BD4; i++ ) {
        if ( !(D_80074BCC[i].h0 & 1) ) {
            D_80074BCC[i] = *a1;
            return i + 1;
        }
    }
    if ( D_80074BD4 >= D_80074BD0 ) {
        old = D_80074BCC;
        D_80074BD0 += 10;
        D_80074BCC = func_8001FE30(D_80074BD0 * 12);
        func_80023ED0(D_80074BCC, old, D_80074BD4 * 12);
        func_8001FD70(old);
    }
    D_80074BCC[D_80074BD4] = *a1;
    return ++D_80074BD4;
}
