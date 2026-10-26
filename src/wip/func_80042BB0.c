/* 4 differing bytes: retail delay slot nop after last sw (ours fills it); static inline init variant identical. wall: schedule (delay slot) */
#include "so2.h"
static inline S_80042BB0 *init(S_80042BB0 *p)
{
    p->p4 = D_8004D310;
    p->w8 = 0;
    return p;
}
S_80042BB0 *func_80042BB0(S_80042BB0 *a0)
{
    return init(a0);
}
