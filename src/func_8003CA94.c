#include "so2.h"
unsigned short func_8003CA94(int a, int b, int c, int d, int e) {
    S_8003CA94 *buf;
    unsigned short *p;
    unsigned short n, mode, x, y;
    int r;

    buf = func_80012154(D_8004EBE8, a, b);
    func_800121A8(D_8004EBE8, a, buf, b);
    p = buf->h4;
    n = buf->h2;
    mode = buf->h0 & 3;
    if ( n ) {
        func_80012EC4(p, 0, e);
        p += n;
    }
    x = p[0];
    y = p[1];
    r = func_80012E04(p + 2, mode, 0, c, d, x << ( 2 - mode ), y);
    func_80020E5C(0);
    func_8001FD70(buf);
    return r;
}
