#include "so2.h"
void func_8003F068(S_8003EC54 *a0, int a1)
{
    S_8003EC54_tag *pkt, *pkt2;
    S_8003EC54_ot *ot, *ot2;

    pkt = func_8003C434(a0->p0->p4, 32);
    func_8003E8C4(a0, pkt, 96, 96, 128, 40, 0, 0, 26, 30912, a1, 0);
    pkt2 = pkt + 5;
    ot = a0->p0;
    pkt->addr = ot->p0C->addr;
    ot->p0C->addr = (unsigned int)pkt;
    ot2 = a0->p0;
    pkt2->addr = ot2->p0C->addr;
    ot2->p0C->addr = (unsigned int)pkt2;
}
