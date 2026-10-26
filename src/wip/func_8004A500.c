/* 75 bytes differ: s0/s1 register assignment (retail v=s0 pkt=s0 masks s1,s2); wall class: register/schedule */
#include "so2.h"
typedef struct { char pad0[8]; short h8, hA, hC, hE; char pad10[0xC]; S_8003EC54_ot *p1C; } S_8004A500;
void func_8004A500(S_8004A500 *a0)
{
    int v;
    short *r;
    S_8003EC54_tag *pkt;
    S_8003EC54_ot *ot;

    r = D_1F8001EC_P;
    v = a0->h8 + 8;
    if ( v < 0 ) v = 0;
    *r = v;
    v = a0->hA + 4;
    if ( v < 0 ) v = 0;
    D_1F8001EE = func_8003C548() + v;
    v = a0->hC - 16;
    if ( v + a0->h8 + 8 > 320 ) v = 312 - a0->h8;
    if ( v < 0 ) v = 0;
    D_1F8001F0 = v;
    v = a0->hE - 8;
    if ( v + a0->hA + 4 > 240 ) v = 236 - a0->hA;
    if ( v < 0 ) v = 0;
    D_1F8001F2 = v;
    pkt = func_8003C434( a0->p1C->p4, 12 );
    func_80021C20( pkt, D_1F8001EC_P );
    ot = a0->p1C;
    pkt->addr = ot->p0C->addr;
    ot->p0C->addr = (unsigned int)pkt;
    pkt = func_8003C434( a0->p1C->p4, 12 );
    func_80021D30( pkt, 0, 0, 31, 0 );
    ot = a0->p1C;
    pkt->addr = ot->p0C->addr;
    ot->p0C->addr = (unsigned int)pkt;
}
