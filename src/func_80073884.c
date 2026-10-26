#include "so2.h"
void func_80073884(S_80073884 *a0, int a1)
{
    a0->p4 = D_80074AD0;
    func_8003C0E0(a0->p0C->w8);
    func_8001FD70(a0->p0C);
    if ( a0->p10 ) {
        a0->p10->p4->fn0C(&a0->p10->pad0[a0->p10->p4->h8], 3);
    }
    func_8003823C(a0, a1);
}
