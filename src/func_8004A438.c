#include "so2.h"
void func_8004A438(S_8004A438 *a0)
{
    S_8003EC54_tag *pkt;
    S_8003EC54_ot *ot;

    pkt = func_8003C434(a0->p1C->p4, 12);
    D_1F8001EC = 0;
    D_1F8001EE = func_8003C548();
    D_1F8001F0 = 320;
    D_1F8001F2 = 240;
    func_80021C20(pkt, D_1F8001EC_P);
    ot = a0->p1C;
    pkt->addr = ot->p0C->addr;
    ot->p0C->addr = (unsigned int)pkt;
}
