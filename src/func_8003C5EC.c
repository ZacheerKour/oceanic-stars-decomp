#include "so2.h"
void func_8003C5EC(S_8003C5EC *o, int a1) {
    S_8003C5ECd d;
    S_8006E1A0 r;
    int h, n, sel;
    void *buf;

    h = func_800100C4() + 16;
    o->w8 = func_800100C4() + 1;
    func_8001440C(func_800100C4() + 4, 0);
    n = func_800104D4(3) + 18;
    buf = func_8001FE30(func_80011B98(h, n));
    o->p4 = buf;
    func_80011ABC(h, buf, n, 0);
    d.h0 = 0;
    d.h2 = 0;
    d.h4 = 640;
    d.h6 = 480;
    d.h8 = 0;
    d.hA = 0;
    d.hC = 256;
    d.hE = 240;
    d.b10 = 1;
    d.b11 = 1;
    r.x = 0;
    r.y = 0;
    r.w = 960;
    r.h = 480;
    func_80020FE0(&r, 0, 0, 0);
    func_8002168C(&d);
    func_8003C8F8(o, 170, 56, o->p4, 0, 0, 0);
    func_8001FD70(o->p4);
    buf = func_8001FE30(func_80011B98(h, 17));
    o->p0 = buf;
    func_80011ABC(h, buf, 17, 0);
    func_80011D78(h);
    func_8003C8F8(o, 170, 390, o->p0, 0, 0, 0);
    func_8003C8F8(o, 162, 390, o->p0, a1, 3, 0);
    func_8003C8F8(o, 610, 55, o->p0, a1 + 2, 3, 0);
    func_8001FE50(0);
    func_80020DC4(1);
    while ( 1 ) {
        func_80011E24(h);
        func_8003C8F8(o, 610, 55, o->p0, 6, 3, 1);
        sel = func_80011EB4(h);
        if ( sel == 1 ) {
            if ( func_80011D38(h, a1) == 1 ) break;
            func_80011D78(h);
        }
        switch ( sel ) {
        case 0:
            func_8003C8F8(o, 610, 55, o->p0, 7, 3, 1);
            break;
        case 1:
            func_8003C8F8(o, 610, 55, o->p0, 5, 3, 1);
            break;
        case 2:
            func_8003C8F8(o, 610, 55, o->p0, 8, 3, 1);
            break;
        }
    }
    func_8001FE50(0);
    func_80020DC4(0);
    func_8001FD70(o->p0);
}
