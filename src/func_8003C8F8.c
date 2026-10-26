#include "so2.h"
void func_8003C8F8(S_8003C8F8 *o, int x, int y, int id, int key, int flags, int clear) {
    S_8003C8F8_r r;
    void *buf;
    int n;

    if ( clear ) {
        func_8001FE50( 0 );
        r.x = 0;
        r.y = y - 54;
        r.w = 960;
        r.h = 54;
        func_80020FE0( &r, 0, 0, 0 );
        func_80020E5C( 0 );
        func_8001FE50( 0 );
        func_80020FE0( &r, 0, 0, 0 );
        func_80020E5C( 0 );
    }
    buf = func_80012154( o->w8, id, key );
    func_800121A8( o->w8, id, buf, key );
    n = func_8003C8D8( o, &r, buf );
    if ( flags & 1 ) r.x = ( ( x - r.w * 2 / 3 ) & -2 ) * 3 >> 1;
    else r.x = x * 3 >> 1;
    if ( flags & 2 ) r.y = y - r.h;
    else r.y = y;
    func_80021108( &r, n );
    func_80020E5C( 0 );
    func_8001FD70( buf );
}
