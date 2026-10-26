/* 4 differing bytes: retail leaves the delay slot nop after the final store (ours fills it); return v0 / int variants unchanged. wall: register/schedule (delay slot) */
#include "so2.h"
int func_80041DA0(S_80032918 *a0)
{
    a0->w[2] = 1;
    return 1;
}
