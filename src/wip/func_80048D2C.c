/* 112 differing bytes: wall control-flow: retail loop is top-tested (not rotated), a4=(i==0)<<2 kept via sltiu, a1=4 set in delay slot then =80; gcc merges into constant s3=4 */
#include "so2.h"
void func_80048D2C(S_80048D2C *a0)
{
    int i;
    int x;

    func_8004A438(a0);
    i = 0;
    x = 0;
    while ( i < 7 ) {
        func_8004A3D0(a0, i ? 4 : 80, x, a0->w20 * 7 + i + 150, (i == 0) << 2);
        x += 14;
        i++;
    }
    func_8004A500(a0);
    func_8004950C(a0);
}
