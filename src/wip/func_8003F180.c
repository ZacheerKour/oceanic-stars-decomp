/* ~7 differing bytes (same wall as 8003EC54/8003ED2C): mask constant register order (retail 0x00FFFFFF in $4, 0xFF000000 in $5). Not compiled to confirm; same body with 144,47,133,30784,80,88 */
#include "so2.h"
void func_8003F180(S_8003EC54 *a0, int a1)
{
    S_8003EC54_tag *pkt;
    S_8003EC54_ot *ot;

    pkt = func_8003C434(a0->p0->p4, 40);
    func_8003E958(a0, pkt, 80, 88, 144, 47, 0, 0, 133, 30784, a1, 0);
    ot = a0->p0;
    pkt->w0 = ( pkt->w0 & 0xFF000000 ) | ( ot->p0C->w0 & 0xFFFFFF );
    ot->p0C->w0 = ( ot->p0C->w0 & 0xFF000000 ) | ( (unsigned int)pkt & 0xFFFFFF );
}
