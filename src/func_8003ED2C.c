#include "so2.h"
void func_8003ED2C(S_8003EC54 *a0, int a1)
{
    S_8003EC54_tag *pkt;
    S_8003EC54_ot *ot;

    pkt = func_8003C434(a0->p0->p4, 40);
    func_8003E958(a0, pkt, 41, 200, 230, 7, 0, 0, 29, 30912, a1, 0);
    ot = a0->p0;
    pkt->addr = ot->p0C->addr;
    ot->p0C->addr = (unsigned int)pkt;
}
