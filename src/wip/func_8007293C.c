/* 8007293C: differs only in loop strength reduction: ours reduces buf[j] to a pointer giv (a2++ plus separate counter), retail keeps sp+j (class: loop-opt/giv). Everything else (regs, base giv pkt+14, srl5/sll3) lines up; tried j types, bound order, j++ placement, buf size, signed buf. */
#include "so2.h"
void func_8007293C(S_8007293C *a0, int a1, S_8007293Ct *a2, int a3)
{
    unsigned char buf[16];
    S_8007293Cp *pkt;
    S_8003EC54_tag *tbl;
    int x, y, j;
    unsigned int clut;
    unsigned char c;

    pkt = (S_8007293Cp *)func_8003C434( a0->p58->p4, a2->h4 * 20 );
    x = a0->h6A + a2->h0;
    y = a0->h6C + a2->h2;
    tbl = a0->p58->p0C;
    func_800100EC( buf, a1, a2->h4, 32 );
    clut = ( ( a3 + 480 ) << 6 ) | 0x11;
    j = 0;
    while ( 1 ) {
        if ( j >= a2->h4 ) break;
        c = buf[j] - 32;
        j++;

        pkt->u.w0 = 0x4000000;
        pkt->w4 = a0->w10 | 0x64000000;
        pkt->h8 = x;
        x += 8;
        pkt->hA = y;
        pkt->h10 = 8;
        pkt->h12 = 8;
        pkt->bC = c % 32 * 8;
        pkt->bD = c / 32 * 8 - 56;
        pkt->hE = clut;
        pkt->u.tag.addr = tbl[a0->h60].addr;
        tbl[a0->h60].addr = (unsigned int)pkt;
        pkt++;
    }
}
