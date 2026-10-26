/* EXACT (160B) but dummy temp f (assignment inside the call args blocks gcc's merge of (i==0)<<2 with i?4:80 into s3=4); no natural spelling found: MATCH-PENDING. Plain args = 126 differing bytes (wall: CSE/control-flow). */
#include "so2.h"
void func_80048D2C(S_80048D2C *a0)
{
    int i;
    int x;
    int f;

    func_8004A438(a0);
    i = 0;
    x = 0;
    while ( 1 ) {
        if ( i >= 7 ) break;
        func_8004A3D0(a0, i == 0 ? 80 : 4, x, a0->w20 * 7 + i + 150, f = (i == 0) << 2);
        x += 14;
        i++;
    }
    func_8004A500(a0);
    func_8004950C(a0);
}
