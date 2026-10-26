/* 18 differing bytes: retail copies this to v0 before loading the vtable address, stores via v0; local p / static inline init variants unchanged. wall: register/schedule */
#include "so2.h"
static inline S_80047690 *init(S_80047690 *p)
{
    p->p4 = D_8004D458;
    p->w20 = 0;
    return p;
}
S_80047690 *func_80047690(S_80047690 *a0)
{
    func_800493CC(a0, 128);
    return init(a0);
}
