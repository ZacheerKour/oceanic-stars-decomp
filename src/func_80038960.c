#include "so2.h"
int func_80038960(int a, short b, unsigned short c, int d, unsigned short e, short f) {
    S_80038DBC s, *p;

    p = &s;
    p->h0 = c | 2;
    p->h2 = b;
    p->w4 = d;
    if ( d == 0 ) p->h0 = c | 10;
    p->hC = e;
    p->hE = f;
    if ( f == 0 ) p->hE = func_80011B98( D_8004EBE4, b ) / 2048;
    if ( p->w4 == 0 ) p->w4 = (int)func_8001FE30( p->hE << 11 );
    return func_80038A1C( a, p );
}
