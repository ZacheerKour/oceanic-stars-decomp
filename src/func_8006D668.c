#include "so2.h"
short func_8006D668(int a0, short a1)
{
    unsigned short f = D_80074BCC[a1 - 1].h0;
    short v = D_80074BCC[a1 - 1].h2;

    if ( f & 4 ) {
        v = v % 64 * 4;
    } else if ( f & 8 ) {
        v = v % 128 * 2;
    }
    return v;
}
