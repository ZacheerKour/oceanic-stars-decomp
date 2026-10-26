#include "so2.h"
void func_80041D64(void)
{
    S_8002F8A4 *p;

    p = (S_8002F8A4 *)func_8001FD94(12);
    p->p4 = &D_8002F8C4;
    D_8002F8DC = p;
    p->p4 = (void (*)())D_8004D330;
}
