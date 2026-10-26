#include "so2.h"
void func_80074438(S_80074438 *a0, unsigned short a1)
{
    if ( a1 == 0xFFFE ) {
        a0->h2E = 2;
    } else {
        a0->h2E = 3;
        a0->h30 = a1;
    }
    a0->p4->fn1C(&a0->pad0[a0->p4->h18]);
}
