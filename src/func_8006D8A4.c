#include "so2.h"
short func_8006D8A4(int a0, short a1, short a2)
{
    short best;
    short area;
    short i, j, n, r;

    best = 0;
    area = 1024;
    n = D_80074BD4;
    for ( i = 0; i < n; i++ ) {
        if ( D_80074BCC[i].h0 & 2 ) {
            if ( D_80074BCC[i].h6 >= a1 ) {
                if ( D_80074BCC[i].h8 >= a2 ) {
                    if ( D_80074BCC[i].h8 < area ) {
                        best = i + 1;
                        area = D_80074BCC[i].h8;
                    }
                }
            }
        }
    }
    r = best;
    if ( r == 0 ) {
        func_80023F40(D_80074968);
        for ( j = 0; j < D_80074BD4; j++ ) {
        }
        return best;
    }
    D_80074BCC[r - 1].h0 &= ~2;
    func_8006DAC0(a0, D_80074BCC[r - 1].h2, (short)(a2 + D_80074BCC[r - 1].h4), D_80074BCC[r - 1].h6, (short)(D_80074BCC[r - 1].h8 - a2));
    func_8006DAC0(a0, (short)(a1 + D_80074BCC[r - 1].h2), D_80074BCC[r - 1].h4, (short)(D_80074BCC[r - 1].h6 - a1), (short)a2);
    D_80074BCC[r - 1].h6 = a1;
    D_80074BCC[r - 1].h8 = a2;
    return r;
}
