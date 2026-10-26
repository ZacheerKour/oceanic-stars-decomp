/* ~513 bytes differ (logic/structure right: raw-word tag macros, SPRT-like 16-byte prims, giv base a1=pkt+12 reproduced); remaining: register assignment (retail ctx=s5 pkt=s0 pos=s1 tail=s3), prologue order, and retail uses signed slt for the p<str+n test; wall: register/schedule */
#include "so2.h"
typedef struct { unsigned int w0; char pad4[3]; unsigned char b7; unsigned int w8; unsigned int wC; } S_80049CD0p;
typedef struct { char pad0[8]; short h8; short hA; char padC[0xC]; unsigned char b18; char pad19[3]; S_8003EC54_ot *p1C; } S_80049CD0;
extern unsigned char D_8004EAFC[];
int func_80049CD0( S_80049CD0 *a0, int x, int y, unsigned char *str, int color )
{
    int n;
    int mode;
    unsigned int pos;
    unsigned int clut;
    unsigned char *p;
    int c;
    unsigned int code;
    S_80049CD0p *pkt;
    unsigned int *tail;
    unsigned int t;

    x += 8;
    y += 8;
    y += a0->hA;
    x += a0->h8;
    n = 0;
    p = str;
    while ( *p ) {
        p++;
        n++;
    }
    pkt = (S_80049CD0p *)func_8003C434( a0->p1C->p4, n << 4 | 12 );
    if ( pkt == 0 ) {
        return 0;
    }
    pos = x + ( y << 16 );
    mode = 0;
    clut = ( color + 32640 ) << 16;
    tail = (unsigned int *)a0->p1C->p0C;
    p = str;
    while ( p < str + n ) {
        c = *p;
        pkt->b7 = 117;
        if ( c == 64 ) {
            mode = 1 - mode;
            p++;
            continue;
        }
        if ( c < 65 ) {
            if ( c == 10 ) {
                y += a0->b18;
                pos = x + ( y << 16 );
                p++;
                continue;
            }
            pkt->w8 = pos;
            pos += 8;
        } else if ( c < 224 && c >= 222 ) {
            pkt->w8 = pos - 8;
        } else {
            pkt->w8 = pos;
            pos += 8;
        }
        if ( mode && c - 166 < 56 ) {
            code = D_8004EAFC[c + 56];
        } else {
            code = D_8004EAFC[c];
        }
        pkt->wC = ( ( code & 31 ) << 3 ) | ( ( code >> 5 ) << 11 ) | clut;
        t = *tail;
        pkt->w0 = ( t & 0xFFFFFF ) | 0x3000000;
        *tail = ( t & 0xFF000000 ) | ( (unsigned int)pkt & 0xFFFFFF );
        pkt++;
        p++;
    }
    func_80021D30( pkt, 0, 0, 15, 0 );
    pkt->w0 = ( pkt->w0 & 0xFF000000 ) | ( *tail & 0xFFFFFF );
    *tail = ( *tail & 0xFF000000 ) | ( (unsigned int)pkt & 0xFFFFFF );
    return ( pos & 65535 ) - 8 - a0->h8;
}
