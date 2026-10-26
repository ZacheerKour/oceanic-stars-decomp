#include "so2.h"
S_80041100 *func_80041100(S_80041100 *a0)
{
    int i;
    int j;

    i = 0;
    while ( 1 ) {
        if ( i >= 128 ) {
            break;
        }
        for ( j = 0; j < 34; j++ ) {
            a0->r[i].w[j] = 0;
        }
        i++;
    }
    for ( j = 0; j < 24; j++ ) {
        a0->w4400[j] = 0;
    }
    a0->p447C = (S_80042BB0 *)func_8001FD94(16);
    func_8003C57C(a0->p447C, 0x1C000);
    func_8003C5B4(a0->p447C, 128);
    a0->w4460 = 1;
    a0->w4464 = 0;
    return a0;
}
