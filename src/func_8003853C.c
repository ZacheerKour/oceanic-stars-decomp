#include "so2.h"
unsigned int func_8003853C( int a0, int scale, unsigned int rgb ) {
    int r, g, b, d, c;

    r = rgb & 0xFF;
    g = ( rgb >> 8 ) & 0xFF;
    b = ( rgb >> 16 ) & 0xFF;
    scale &= 0xFF;
    if ( scale == 0 ) {
        c = 0;
    } else {
        d = 128 / scale;
        c = ( ( b / d ) << 16 ) + ( ( g / d ) << 8 ) + r / d;
    }
    return c;
}
