#include "so2.h"
void func_80046F8C(S_8003EC54 *a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10)
{
    S_80046F8C *pkt;
    S_8003EC54_ot *ot;

    pkt = (S_80046F8C *)func_8003C434(a0->p0->p4, 32);
    pkt->h8 = a1;
    pkt->hA = a2;
    pkt->h10 = a3;
    pkt->h12 = a4;
    pkt->bC = a5;
    pkt->bD = a6;
    pkt->b4 = a9;
    pkt->b5 = a9;
    pkt->b6 = a9;
    pkt->b7 = a10 ? 102 : 100;
    pkt->u.w0 = 0x4000000;
    pkt->hE = a8;
    ot = a0->p0;
    pkt->u.t.addr = ot->p0C->addr;
    ot->p0C->addr = (unsigned int)pkt;
    pkt += 1;
    func_80021D30( pkt, 0, 0, a7, 0 );
    ot = a0->p0;
    pkt->u.t.addr = ot->p0C->addr;
    ot->p0C->addr = (unsigned int)pkt;
}
