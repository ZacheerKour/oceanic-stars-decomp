/* 6 differing bytes: wall masked/combine: retail narrows the <<1 result as sll16+srl15, ours sll+andi; <<2 branch matches  Wave 10 also tried: v<<=1, v=v+v, v=(unsigned short)(v*2) [6 bytes, best], int v, two ifs instead of else-if: retail sll16/srl15 never produced (ours andi+sll). */
#include "so2.h"
short func_8006D5E0(int a0, short a1)
{
    unsigned short flags;
    unsigned short v;

    flags = D_80074BCC[a1 - 1].h0;
    v = D_80074BCC[a1 - 1].h6;
    if ( flags & 4 ) {
        v *= 4;
    } else if ( flags & 8 ) {
        v *= 2;
    }
    return v;
}
