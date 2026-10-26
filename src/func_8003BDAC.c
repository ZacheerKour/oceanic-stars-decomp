#include "so2.h"
void func_8003BDAC(S_8003BE84 *o, int a, int b, int c, int d, int e, int f, int g, int h) {
    S_8003BDAC_r r;

    o->h64 = ( g << 8 ) / ( c - a );
    o->h66 = e;
    o->h68 = ( ( c - g - h ) << 8 ) / ( c - a );
    o->h6A = 240 - e - f;
    r.x = a;
    r.y = b;
    r.w = c;
    r.h = d;
    func_80020FE0( &r, 0, 0, 0 );
}
