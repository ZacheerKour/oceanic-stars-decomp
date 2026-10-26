#include "so2.h"
S_80039068 *func_80039068(S_80039068 *o) {
    S_8006E1A0 r;
    S_80039068p *p;

    func_8003A174(o);
    o->p4 = D_8004B9C4;
    func_80035F8C(&o->w14);
    o->p10 = func_8006E5E0(func_8001FD94(132));
    o->p10->h60++;
    o->p10->h60++;
    r.x = 40;
    r.y = 40;
    r.w = 160;
    r.h = 160;
    func_8006EBFC(o->p10, r, 0);
    func_8006F08C(o->p10, 8, 8);
    func_8006F01C(o->p10, D_8004B974);
    func_8006F08C(o->p10, 8, 40);
    func_8006F1A0(o->p10, o->w1C);
    return o;
}
