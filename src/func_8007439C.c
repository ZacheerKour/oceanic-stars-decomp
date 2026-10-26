#include "so2.h"
void func_8007439C(S_8007439C *a0, short a1)
{
    if ( a0->b46 == 0 ) {
        a0->h44 = a1;
        a0->b46 = 1;
        func_80074438(a0, a0->h44);
        a0->h44 = 0xFFFF;
    }
}
