#include "so2.h"
void func_80048240(S_800481E4 *a0)
{
    if ( a0->w24 ) {
        a0->w24--;
        a0->e[a0->w24].b0 = 0;
        func_80019B1C(D_8004EBF8, 6, 0, 32576);
    }
}
