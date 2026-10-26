#include "so2.h"
void func_80032840(S_80030CBC *o, int a)
{
    S_80032840e *p;
    short idx;
    int n;
    int i;

    idx = a;
    o->hF4 = idx;
    p = o->p8[idx];
    o->h100 = 0;
    n = p->h4 * o->hE4 / 4;
    o->pF8 = func_8001FE30(n * 2);
    o->pFC = o->pF8;
    for ( i = 0; i < p->h4 * o->hE4 / 4; i++ ) {
        o->pF8[i] = 0;
    }
    func_80032950(o, 0);
    func_80032918(o, o->hEE);
}
