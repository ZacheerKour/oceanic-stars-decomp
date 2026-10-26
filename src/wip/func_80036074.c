/* DIFF 24 bytes: retail does move v0,s0 right after the call and stores all 5 fields via v0 (same quirk as func_8003540C); ours stores via s0, move v0 late. Wall class register/schedule. Tried return o, alias local. */
#include "so2.h"
S_800360BC *func_80036074(S_800360BC *o) { func_80038228(o); o->p4 = &D_8004B728; o->w10 = 0; o->p8 = 0; o->wC = 0; o->w14 = 0; return o; }
