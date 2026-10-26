/* 162 bytes differ (472 total): logic and frame (96) match, only register/schedule noise (retail count in t0 vs ours t1; move a0,s6 hoisted into bnez slot; jal slot holds x-=10); wall: register/schedule. Top-tested while(1) loop lever applied. */
#include "so2.h"
typedef struct { S_8003EC54_tag tag; char pad4[36]; } S_800411A4e;
extern short D_8004E2DC[];
void func_800411A4( S_8003EC54 *a0, int x, int a2, int val, int count, int a6, int a7 )
{
    S_800411A4e *pkt;
    S_800411A4e *p;
    S_8003EC54_ot *ot;
    int i;
    int d;

    pkt = (S_800411A4e *)func_8003C434( a0->p0->p4, count * 40 );
    x += count * 10;
    p = pkt - 1;
    i = 0;
    while ( 1 ) {
        if ( i >= count ) {
            break;
        }
        d = val % 10;
        if ( d == 0 && val == 0 && i != 0 ) {
            break;
        }
        p++;
        func_8003E958( a0, pkt, x, a2, D_8004E2DC[d + 1] - D_8004E2DC[d], 11, D_8004E2DC[d], 50, 140, a6 << 6, a7, 0 );
        x -= 10;
        ot = a0->p0;
        p->tag.addr = ot->p0C->addr;
        ot->p0C->addr = (unsigned int)pkt;
        i++;
        pkt++;
        val /= 10;
    }
}
