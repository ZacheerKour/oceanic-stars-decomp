/* 183 differing bytes: wall schedule: body/size match; retail computes w4 (or) before the glyph row/col math and keeps srl5/sll3 uncombined */
#include "so2.h"
void func_800727A0(S_80072FBC *a0, char *a1, S_800727A0r *a2)
{
    S_800727A0p *p;
    S_8003EC54_tag *ot;
    int t;
    unsigned char row, c;
    short i;
    int x;
    short y;

    p = func_8003C434(a0->p58->p4, func_80035BC8(a1) * 20);
    x = a0->h6A + a2->x;
    y = a0->h6C + a2->y;
    ot = a0->p58->p0C;
    i = 0;
    while ( 1 ) {
        if ( i >= func_80035BC8(a1) ) break;
        c = a1[i];
        p->u.w0 = 0x04000000;
        p->h8 = x;
        x += 8;
        p->hA = y;
        p->h10 = 8;
        p->h12 = 8;
        p->w4 = a0->w10 | 0x64000000;
        t = (c - 32) & 0xFF;
        p->b0C = (t & 0x1F) << 3;
        p->b0D = (t >> 5 << 3) - 56;
        p->h0E = a0->h1A;
        p->u.tag.addr = ot[a0->h60].addr;
        ot[a0->h60].addr = (unsigned int)p;
        i++;
        p++;
    }
}
