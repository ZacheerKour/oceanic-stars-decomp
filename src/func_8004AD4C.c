#include "so2.h"
void func_8004AD4C(void)
{
    S_8006E1A0 r;

    if ( D_8004EC18 != 0 || D_8004EC1A != 0 ) {
        r.x = 960;
        r.w = 64;
        r.y = 256;
        r.h = D_8004EC1A + 12;
        if ( r.h >= 256 ) {
            r.h = 256;
        }
        func_80021108(&r, D_8004EC24);
    }
}
