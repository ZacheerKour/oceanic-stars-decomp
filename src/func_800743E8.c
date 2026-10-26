#include "so2.h"
void func_800743E8(S_8007439C *a0)
{
    if ( a0->b46 == 0 ) {
        a0->h44 = 0xFFFE;
        a0->b46 = 1;
        func_80074438(a0, a0->h44);
        a0->h44 = 0xFFFF;
    }
}
