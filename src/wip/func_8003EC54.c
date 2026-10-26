/* 7 differing bytes: wall register/schedule: retail allocates the 0x00FFFFFF mask to a0 ($4) and 0xFF000000 to a1 ($5), ours the reverse; addPrim-style tag link. Tried operand swaps, local for next addr */
#include "so2.h"
void func_8003EC54(S_8003EC54 *a0, int a1)
{
    S_8003EC54_tag *pkt;
    S_8003EC54_ot *ot;

    pkt = func_8003C434(a0->p0->p4, 40);
    func_8003E958(a0, pkt, 86, 144, 136, 16, 0, 0, 28, 30848, a1, 0);
    ot = a0->p0;
    pkt->w0 = ( pkt->w0 & 0xFF000000 ) | ( ot->p0C->w0 & 0xFFFFFF );
    ot->p0C->w0 = ( ot->p0C->w0 & 0xFF000000 ) | ( (unsigned int)pkt & 0xFFFFFF );
}
