/* wip: ~register residue only (buf address cached in an s-reg: retail keeps q in s0, no buf reg; needs types S_800378F0/S_800378F0e + protos from the so2.h appended block, see git history) */
#include "so2.h"
void func_800378F0(int a, S_800378F0 *s) {
    S_80036028 o;
    char buf[256];
    S_800378F0e e[15];
    S_800378F0e *p;
    S_80036028 *q;
    int n;
    short i;

    func_80035F8C(&o);
    q = &o;
    if ( D_8004B760 == 37 ) {
        func_80035C68(q, &D_8004B760);
    } else {
        func_80035BEC(o.p8, &D_8004B760);
        if ( func_80035BC8(o.p8) >= 128 ) func_80023F40(&D_8004B758);
    }
    if ( D_8004B764 == 37 ) {
        func_80035C68(q, &D_8004B764);
    } else {
        func_80035BEC(q->p8, &D_8004B764);
        if ( func_80035BC8(q->p8) >= 128 ) func_80023F40(&D_8004B758);
    }
    func_80035E58(q, buf, s->h784);
    func_80035BEC(q->p8, buf);
    if ( func_80035BC8(q->p8) >= 128 ) func_80023F40(&D_8004B758);
    if ( D_8004B800 == 37 ) {
        func_80035C68(q, &D_8004B800);
    } else {
        func_80035BEC(q->p8, &D_8004B800);
        if ( func_80035BC8(q->p8) >= 128 ) func_80023F40(&D_8004B758);
    }
    p = e;
    n = 0;
    if ( func_8004B0D4(o.p8, p) == p ) {
        do {
            n++;
            p++;
        } while ( func_8004B0B4(p) == p );
    }
    s->h78C = 15;
    s->h78A = n;
    for ( i = 0; i < s->h78A; i++ ) s->h78C -= e[i].size / 8192;
    func_80035FDC(&o, 2);
}
