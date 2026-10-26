/* wip: ~20 differing bytes; wall referent/CSE: retail reloads w24/w28 after storing them (lw a0,36(v0)) and defers the copies; plain C forwards the store. gets close but reorders regs and is a fakematch. */
#include "so2.h"
typedef struct { char pad0[4]; void *p4; char pad8[0x18]; int w20; int w24; int w28; int w2C; int w30; int w34; } S_800478E8;
extern char D_8004D3F8[];
S_800478E8 *func_800478E8(S_800478E8 *a0)
{
    func_800493CC(a0, 512);
    a0->w24 = 2;
    a0->w2C = a0->w24;
    a0->w28 = 0;
    a0->w30 = a0->w28;
    a0->p4 = D_8004D3F8;
    a0->w20 = 0;
    a0->w34 = 0;
    return a0;
}
