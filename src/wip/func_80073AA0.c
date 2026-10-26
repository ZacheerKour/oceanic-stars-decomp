/* 31 differing bytes: retail computes o+h38 (addu) before loading fn (lw 60) and keeps vt in v1; ours schedules addu into the jalr delay slot. wall: schedule; tried local vt/t/fn/off variants */
#include "so2.h"
void func_80073AA0(S_80073AA0 *o) { o->p4->fn3C(&o->pad0[o->p4->h38]); }
