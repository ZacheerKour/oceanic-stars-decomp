/* ~109 bytes differ: tp expression association (0x120 merged with y part); wall: control/expression shape */
#include "so2.h"
#define TP(x, y) ( ( 2 << 7 ) | ( 1 << 5 ) | ( ( ( y ) & 0x100 ) >> 4 ) | ( ( ( x ) & 0x3FF ) >> 6 ) | ( ( ( y ) & 0x200 ) << 2 ) )
void func_8003EE04(S_8003EC54 *a0, int a1, int a2, int a3, int a4)
{
    S_8003EC54_tag *pkt, *pkt2;
    S_8003EC54_ot *ot;

    pkt = func_8003C434(a0->p0->p4, 32);
    func_8003E8C4(a0, pkt, 0, 0, 256, 240, 0, 0,
                  TP( a1, a2 ), 30720, a3, a4);
    ot = a0->p0;
    pkt->addr = ot->p0C->addr;
    ot->p0C->addr = (unsigned int)pkt;
    pkt2 = pkt + 5;
    ot = a0->p0;
    pkt2->addr = ot->p0C->addr;
    ot->p0C->addr = (unsigned int)pkt2;
    pkt = func_8003C434(a0->p0->p4, 32);
    func_8003E8C4(a0, pkt, 256, 0, 64, 240, 0, 0,
                  TP( a1 + 256, a2 ), 30720, a3, a4);
    ot = a0->p0;
    pkt->addr = ot->p0C->addr;
    ot->p0C->addr = (unsigned int)pkt;
    pkt2 = pkt + 5;
    ot = a0->p0;
    pkt2->addr = ot->p0C->addr;
    ot->p0C->addr = (unsigned int)pkt2;
}
