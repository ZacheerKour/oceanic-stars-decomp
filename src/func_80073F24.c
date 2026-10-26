#include "so2.h"
void func_80073F24(S_80073F24 *a0)
{
    if ( a0->h40 != -1 && (unsigned short)(a0->p3C->h2E - 2) < 2 ) {
        a0->p4->fn54(&a0->pad0[a0->p4->h50], a0->h40, a0->p3C);
        a0->h40 = -1;
    }
    a0->w28 = 0;
    a0->w24 = 0;
    if ( func_80036658(func_8002FFE4()->wC) == a0 ) {
        a0->p4->fn5C(&a0->pad0[a0->p4->h58]);
    }
    func_80036240(a0->p10);
    if ( a0->h36 != 0 ) {
        func_80019B1C(D_8004EBF8, a0->h36, 0, 32576);
        a0->h36 = 0;
    }
}
