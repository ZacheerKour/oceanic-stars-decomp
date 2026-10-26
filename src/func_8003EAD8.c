#include "so2.h"
void func_8003EAD8(S_8003EC54 *a0, int a1)
{
    S_8003EC54_tag *pkt, *pkt2;
    S_8003EC54_ot *ot, *ot2;

    pkt = func_8003C434(a0->p0->p4, 80);
    func_8003E958(a0, pkt, 0, 0, 255, 240, 0, 0, 13, 30976, a1, 0);
    pkt2 = pkt + 10;
    ot = a0->p0;
    pkt->addr = ot->p0C->addr;
    ot->p0C->addr = (unsigned int)pkt;
    func_8003E958(a0, pkt2, 255, 0, 64, 240, 0, 0, 14, 30976, a1, 0);
    ot2 = a0->p0;
    pkt2->addr = ot2->p0C->addr;
    ot2->p0C->addr = (unsigned int)pkt2;
}
