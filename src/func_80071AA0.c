#include "so2.h"
void func_80071AA0(S_80071AA0o *a0, S_80071AA0 *a1)
{
    S_8006E1A0 r;
    unsigned short h;
    char *p;
    int w;

    p = a1->p14;
    w = a1->h18;
    r = a1->r0C;
    if ( a1->h1A >= 255 ) {
        h = a0->h1A;
        a0->h1A = 30929;
        func_800727A0(a0, D_800749AC, &r);
        a0->h1A = h;
    } else {
        r.w = 9;
        func_8007293C(a0, p, &r, 1);
        func_800721C0(a0, a1->r0C.x, a1->r0C.y + 3, 72, w, 100);
    }
}
