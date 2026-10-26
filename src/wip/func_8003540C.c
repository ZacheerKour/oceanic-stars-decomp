/* DIFF 8 bytes: retail stores through v0 (move v0,s0 after the call, returns o); ours stores through s0 then moves v0 at the end. Wall class register/schedule. Tried plain return o, local alias r. */
#include "so2.h"
S_8003540C *func_8003540C(S_8003540C *o) { func_80038644(&o->w8); o->h0 = 0; o->h10 = 0; o->w5C = 0; o->w60 = 0; return o; }
