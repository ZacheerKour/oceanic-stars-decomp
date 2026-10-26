/* 28 differing bytes: same this-copy-in-v0 pattern as 80047690. wall: register/schedule */
#include "so2.h"
static inline S_8006D324 *init(S_8006D324 *p)
{
    p->h1C |= 0x1000;
    p->p4 = D_800749F8;
    return p;
}
S_8006D324 *func_80072AD4(S_8006D324 *a0)
{
    func_8006E5E0(a0);
    return init(a0);
}
