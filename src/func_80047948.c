#include "so2.h"
void func_80047948( S_80047948 *a0, S_80047948b *a1, int a2, int dir )
{
    int x;
    int y;
    int i;

    if ( a0->w52 == 0 ) {
        x = a0->w36;
        y = a0->w40;
        x += a2;
        if ( x < 0 ) {
            if ( y > 0 ) {
                x = 9;
                dir = -1;
            } else {
                x = 4;
            }
        }
        if ( x >= 10 || ( x >= 5 && y == 0 ) ) {
            x = 0;
            if ( y < D_8004E458[a0->w32] ) {
                dir = 1;
            }
        }
        if ( dir > 0 ) {
            if ( y == 0 ) {
                if ( a1->w36 < 7 ) {
                    y = 1;
                    for ( i = 0; i < 10; i++ ) {
                        if ( D_8004EAF0[i] == x ) {
                            x = i;
                            break;
                        }
                    }
                }
            } else {
                y++;
                if ( a1->w36 == 7 ) {
                    if ( y > 0 ) {
                        y = 0;
                    }
                } else if ( y > D_8004E458[a0->w32] ) {
                    y = D_8004E458[a0->w32];
                }
            }
        } else if ( dir < 0 ) {
            if ( y == 1 ) {
                x = D_8004EAF0[x];
                y = 0;
            } else {
                y--;
            }
            if ( y < 0 ) {
                y = 0;
            }
        }
        if ( a0->w36 != x || a0->w40 != y ) {
            func_80019B1C( D_8004EBF8, 1, 0, 32576 );
        }
        a0->w44 = x;
        a0->w48 = y;
        a0->w52 = 3;
    }
}
