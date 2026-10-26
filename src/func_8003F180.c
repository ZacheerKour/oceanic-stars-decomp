#include "so2.h"
void func_8003F180(S_8003EC54 *a0, int a1)
{
    S_8003EC54_tag *pkt;
    S_8003EC54_ot *ot;

    pkt = func_8003C434(a0->p0->p4, 40);
    func_8003E958(a0, pkt, 80, 88, 144, 47, 0, 0, 133, 30784, a1, 0);
    ot = a0->p0;
    pkt->addr = ot->p0C->addr;
    ot->p0C->addr = (unsigned int)pkt;
}
