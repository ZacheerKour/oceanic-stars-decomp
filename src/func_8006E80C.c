#include "so2.h"
void func_8006E80C(S_8006E80C *a0)
{
    int i;

    for ( i = 0; i < a0->h16; i++ ) {
        if ( a0->p50[i].h2 & 2 ) {
            if ( a0->p50[i].h0 == 5 ) {
                if ( a0->p50[i].p8 != 0 ) {
                    func_80067DE4(a0->p50[i].p8, 3);
                }
            } else {
                func_8001FD70(a0->p50[i].p8);
            }
        }
        if ( a0->p50[i].h0 == 2 ) {
            func_800323BC(a0->p54, a0->p50[i].h4);
        }
    }
    if ( a0->p54 != 0 ) {
        func_80032270(a0->p54, 3);
        a0->p54 = 0;
    }
    if ( a0->p5C != 0 ) {
        func_8001FD70(a0->p5C);
        a0->p5C = 0;
    }
    func_8001FD70(a0->p50);
    a0->p50 = 0;
    func_8006E7E4(a0);
}
