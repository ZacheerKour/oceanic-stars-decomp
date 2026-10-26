#include "so2.h"
S_800368EC *func_800368EC(S_800368EC *o) {
    int i;

    func_80038228(o);
    o->e[0].p4 = D_8004B808;
    D_8004EFB4 = o;
    func_8001445C(D_8004DDA0, func_80036BBC);
    func_80024B50();
    o->wFB0[0] = func_80024AC0(0xF4000001, 4, 0x2000, 0);
    o->wFB0[1] = func_80024AC0(0xF4000001, 0x8000, 0x2000, 0);
    o->wFB0[2] = func_80024AC0(0xF4000001, 0x100, 0x2000, 0);
    o->wFB0[3] = func_80024AC0(0xF4000001, 0x2000, 0x2000, 0);
    o->wFB0[4] = func_80024AC0(0xF0000011, 4, 0x2000, 0);
    o->wFB0[5] = func_80024AC0(0xF0000011, 0x8000, 0x2000, 0);
    o->wFB0[6] = func_80024AC0(0xF0000011, 0x100, 0x2000, 0);
    o->wFB0[7] = func_80024AC0(0xF0000011, 0x2000, 0x2000, 0);
    func_80024B00(o->wFB0[0]);
    func_80024B00(o->wFB0[1]);
    func_80024B00(o->wFB0[2]);
    func_80024B00(o->wFB0[3]);
    func_80024B00(o->wFB0[4]);
    func_80024B00(o->wFB0[5]);
    func_80024B00(o->wFB0[6]);
    func_80024B00(o->wFB0[7]);
    func_80024B60();
    o->hFA6 = 0;
    o->hFAE = 0;
    o->hFA4 = 0;
    for ( i = 0; i < 2; i++ ) {
        o->e[i].h78C = 1;
        o->e[i].h78E = 0;
        o->e[i].h790 = i;
        o->e[i].h792 = i << 4;
    }
    return o;
}
