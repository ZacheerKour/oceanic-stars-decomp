#include "so2.h"
void func_8006FD10(S_8006E80C *a0, S_8006E80Ce *a1)
{
    S_8006E80Ce *old;

    if ( a0->h16 >= a0->h14 ) {
        old = a0->p50;
        a0->h14 += 10;
        a0->p50 = func_8001FE30(a0->h14 * 36);
        func_80023ED0(a0->p50, old, a0->h16 * 36);
        func_8001FD70(old);
    }
    a0->p50[a0->h16] = *a1;
    a0->h16++;
}
