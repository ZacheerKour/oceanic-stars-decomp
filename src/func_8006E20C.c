#include "so2.h"
void func_8006E20C(S_8006E20C *a0)
{
    int i;
    S_8006E20C *e;
    S_8006E20C *old;

    a0->h0 |= 1;
    i = 0;
    while ( 1 ) {
        if ( i >= D_80074BD8 ) break;
        e = D_80074BE0 + i;
        if ( !(e->h0 & 1) ) {
            *e = *a0;
            return;
        }
        i++;
    }
    if ( D_80074BD8 >= D_80074BDC ) {
        old = D_80074BE0;
        D_80074BDC += 10;
        D_80074BE0 = func_8001FE30(D_80074BDC * 24);
        func_80023ED0(D_80074BE0, old, D_80074BD8 * 24);
        func_8001FD70(old);
    }
    D_80074BE0[D_80074BD8] = *a0;
    D_80074BD8++;
}
