#include "so2.h"
void func_80048A1C(S_80048A1C *a0)
{
    int i;
    int y;
    int f;
    int c;
    short t, u;
    S_8004D37C ta, tb;
    S_80046F8C *pkt;
    S_8003EC54_tag *pk2;
    S_8003EC54_ot *ot, *ot2;

    func_8004A438( a0 );
    i = 0;
    y = 0;
    while ( 1 ) {
        if ( i >= 7 ) break;
        if ( i == 0 ) {
            c = 72;
        } else {
            c = 8;
        }
        f = ( i == 0 ) << 2;
        func_8004A3D0( a0, c, y, a0->w20 * 7 + i + 58, f );
        y += 14;
        i++;
    }
    func_8004A500( a0 );
    ta = D_8004D37C;
    tb = D_8004D388;
    pkt = (S_80046F8C *)func_8003C434( a0->p1C->p4, 20 );
    t = ta.w[a0->w20] + 8;
    pkt->h8 = a0->h8 + t;
    u = tb.w[a0->w20] + 8;
    pkt->hA = a0->hA + u;
    pkt->h10 = 14;
    pkt->h12 = 11;
    pkt->bD = 160;
    pkt->b7 = 101;
    pkt->u.w0 = 0x4000000;
    pkt->bC = 0;
    pkt->hE = 30729;
    ot = a0->p1C;
    pkt->u.t.addr = ot->p0C->addr;
    ot->p0C->addr = (unsigned int)pkt;
    pk2 = func_8003C434( a0->p1C->p4, 12 );
    func_80021D30( pk2, 0, 0, 7, 0 );
    ot2 = a0->p1C;
    pk2->addr = ot2->p0C->addr;
    ot2->p0C->addr = (unsigned int)pk2;
    func_8004950C( a0 );
}
