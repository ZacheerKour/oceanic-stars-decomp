/* DIFF ~18 bytes: retail frame is 72 (ours 32): 40 bytes of unused locals (dead object) live in the original, and "sw v0,12(s0)" precedes jal 800306a4 (not in its delay slot). Not reproducible without a dummy local. Wall class CFG/frame (dead local). Branch order !f()/else matches retail layout. */
#include "so2.h"
int func_800305C4(S_800305C4 *o)
{
    S_800305C4q *q;

    o->p10 = func_8003540C(func_8001FD94(0x70));
    if ( o->h18 == 0 ) {
        o->p14 = func_800368EC(func_8001FD94(4048));
        o->w2C = 0;
        func_8006DFC8();
        o->w30 = 0;
        func_80068E40();
    }
    q = o->p4;
    q->pC = func_8003C144(q->p8);
    func_800306A4(o);
    if ( !func_8003562C(o->p10) ) {
        func_80036240(o->pC);
        func_8003083C(o);
    } else {
        D_8004DD90 = 7;
    }
    func_8006DFE8();
    return D_8004DDC4->b16;
}
