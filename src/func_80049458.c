#include "so2.h"
void func_80049458(S_80049458 *a0, int a1)
{
    a0->p4 = D_8004D548;
    func_8003C07C(a0->p1C->p4);
    func_8003C0E0(a0->p1C->w8);
    func_8001FD70(a0->p1C);
    if ( a1 & 1 ) {
        func_8001FD70(a0);
    }
}
