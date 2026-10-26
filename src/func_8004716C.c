#include "so2.h"
void func_8004716C(S_80048D2C *a0)
{
    int n;
    int i;
    int x;
    int f;
    int c;

    n = 7;
    if ( a0->w20 == 2 ) {
        n = 5;
    }
    func_8004A438(a0);
    i = 0;
    x = 0;
    while ( 1 ) {
        if ( i >= n ) {
            break;
        }
        if ( i == 0 ) {
            c = 80;
        } else {
            c = 8;
        }
        f = (i == 0) << 2;
        func_8004A3D0(a0, c, x, a0->w20 * 7 + i, f);
        x += 14;
        i++;
    }
    func_8004A500(a0);
    func_8004950C(a0);
}
