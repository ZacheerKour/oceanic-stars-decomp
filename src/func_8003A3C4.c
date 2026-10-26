#include "so2.h"
void func_8003A3C4(S_8003A3C4 *o) {
    S_8003A3C4s *s;
    S_8003A3C4s *r;
    int i;

    s = func_80012154( D_8004DD9C, D_8004BCC4, 0 );
    func_800121A8( D_8004DD9C, D_8004BCC4, s, 0 );
    func_8001FD70( o->p0 );
    o->p0 = func_8001FE30( 13184 );
    r = s;
    for ( i = 0; i < 824; i++ ) {
        o->p0[i].h0 = r->b0 + ( r->b1 << 8 );
        o->p0[i].h2 = r->b2 + ( r->b3 << 8 );
        o->p0[i].h4 = r->b4 + ( r->b5 << 8 );
        o->p0[i].h6 = r->b6 + ( r->b7 << 8 );
        o->p0[i].h8 = r->b8 + ( r->b9 << 8 );
        o->p0[i].hA = r->bA + ( r->bB << 8 );
        o->p0[i].hC = r->bC;
        o->p0[i].hE = r->bD;
        r++;
    }
    func_8001FD70( s );
}
