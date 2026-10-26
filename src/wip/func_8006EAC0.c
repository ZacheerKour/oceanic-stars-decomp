/* 85 differing bytes: wall register/schedule+referent: retail packs halfwords into $5/$6 as ints (lh loads, sum not narrowed), ours (struct-by-value r) narrows sum to short (lhu). a1/a2 are int params. unresolved */
#include "so2.h"
void func_8006EAC0(S_8006F1A0 *a0, int a1, int a2, S_8006EAC0 *a3, int a4)
{
    S_8006F88C_r r;

    r.w = a1;
    r.h = a2;
    r.x = 156 - a1 / 2;
    r.y = a3->h64 + a3->h68;
    func_8006EBFC(a0, r, a4);
}
