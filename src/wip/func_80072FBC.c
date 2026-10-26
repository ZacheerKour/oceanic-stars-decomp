/* wall schedule/CSE: body matches in shape; retail hoists the stack-arg colour constants into s2/s3/s5 before the alloc call and keeps p+9 offsets in s4; ours loads them after */
#include "so2.h"
void func_80072FBC(S_80072FBC *a0)
{
    S_8003EC54_tag *p;
    int c0, c1, c2, c3, c4;

    c0 = 0x3F0000;
    c1 = 0x7F0000;
    c2 = 0x8F8F8F;
    p = func_8003C434(a0->p58->p4, 144);
    func_80073204(a0, p, c0, c1, c0, c1, a0->h6A, a0->h6C, (short)a0->h6E / 2, a0->h70 - 2, a0->h60);
    func_80073204(a0, p + 9, c1, c2, c1, c2, a0->h6A + (short)a0->h6E / 2, a0->h6C, (short)a0->h6E / 2, a0->h70 - 2, a0->h60);
    c3 = 0x1F0000;
    c4 = 0x474747;
    func_80073204(a0, p + 18, c3, c0, c3, c0, a0->h6A, a0->h6C + a0->h70 - 2, (short)a0->h6E / 2, 2, a0->h60);
    func_80073204(a0, p + 27, c0, c4, c0, c4, a0->h6A + (short)a0->h6E / 2, a0->h6C + a0->h70 - 2, (short)a0->h6E / 2, 2, a0->h60);
}
