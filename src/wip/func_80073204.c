/* 116 differing bytes: wall schedule: retail hoists all stack-arg loads above the POLY_G4 stores; source-order permutations (5 tried) do not move it */
#include "so2.h"
void func_80073204(S_80073204 *a0, S_80073204p *a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10)
{
    a1->w4 = a2 | 0x38000000;
    a1->wC = a3;
    a1->u.w0 = 0x08000000;
    a1->h8 = a6;
    a1->hA = a7;
    a1->h10 = a6 + a8;
    a1->h12 = a7;
    a1->w14 = a4;
    a1->w1C = a5;
    a1->h18 = a6;
    a1->h1A = a7 + a9;
    a1->h20 = a6 + a8;
    a1->h22 = a7 + a9;
    a1->u.tag.addr = a0->p58->pC[a10].addr;
    a0->p58->pC[a10].addr = (unsigned int)a1;
}
