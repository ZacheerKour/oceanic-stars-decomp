/* ~260 bytes differ: register assignment (retail s1=x s0=y s2=a4 s3=w s4=a0); wall class: register */
#include "so2.h"
typedef struct { union { S_8003EC54_tag t; int w0; } u; unsigned int w4, w8, wC; } S_8004A0B0;
typedef struct { char pad0[8]; short h8, hA; char padC[0x10]; S_8003EC54_ot *p1C; } S_8004A0B0o;
void func_8004A0B0(S_8004A0B0o *a0, int a1, int a2, int a3, unsigned int a4)
{
    int x, y, w;
    S_8004A0B0 *pkt;
    S_8003EC54_ot *ot;

    x = a1 + 8 + a0->h8;
    y = a2 + 20 + a0->hA;
    w = func_8004A2BC( a0, a3 );
    pkt = (S_8004A0B0 *)func_8003C434( a0->p1C->p4, 16 );
    pkt->u.w0 = 0x3000000;
    pkt->w4 = a4 | 0x40000000;
    pkt->w8 = x | ( y << 16 );
    pkt->wC = ( x + w ) | ( y << 16 );
    ot = a0->p1C;
    pkt->u.t.addr = ot->p0C->addr;
    ot->p0C->addr = (unsigned int)pkt;
    pkt = (S_8004A0B0 *)func_8003C434( a0->p1C->p4, 16 );
    pkt->u.w0 = 0x3000000;
    pkt->w4 = 0x40000000;
    pkt->w8 = ( x + 1 ) | ( ( y + 1 ) << 16 );
    pkt->wC = ( x + w + 1 ) | ( ( y + 1 ) << 16 );
    ot = a0->p1C;
    pkt->u.t.addr = ot->p0C->addr;
    ot->p0C->addr = (unsigned int)pkt;
}
