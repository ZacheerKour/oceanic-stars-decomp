#include "so2.h"
void func_800476D0(S_800476D0 *a0)
{
    int i;
    int x;
    int f;

    func_8004A438(a0);
    if ( a0->w20 != 0 ) {
        a0->h8 = 312 - a0->hC;
    } else {
        a0->h8 = 8;
    }
    i = 0;
    x = 0;
    while ( 1 ) {
        if ( i >= 7 ) {
            break;
        }
        f = (i == 0) << 2;
        func_8004A3D0(a0, 4, x, a0->w20 * 7 + i + 32, f);
        x += 14;
        i++;
    }
    func_8004A500(a0);
    func_80047228(a0);
}
