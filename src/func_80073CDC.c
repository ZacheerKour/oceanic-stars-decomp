#include "so2.h"
void func_80073CDC(S_80073CDC *a0, S_80073CDC_b *a1, unsigned short a2, short a3)
{
    if ( a3 >= 0 && a3 < a1->p5C->h2 ) {
        a0->p4->fn44(&a0->pad0[a0->p4->h40], a1, a2);
    }
}
