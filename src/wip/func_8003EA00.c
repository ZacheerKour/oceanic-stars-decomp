/* wall: register residue, 7 differing bytes: retail keeps q=o->p0 in a2 and the 0xFFFFFF mask in a0, ours swaps them; tried operand order, local order */
#include "so2.h"
void func_8003EA00(S_8003EA00 *o, int a) {
    S_8003E958 *p;
    S_8003EA00a *q;

    p = func_8003C434( o->p0->p4, 40 );
    func_8003E958( o, p, 124, 64, 72, 94, 0, 0, 138, 30720, a, 0 );
    q = o->p0;
    p->w0 = ( p->w0 & 0xFF000000 ) | ( q->pC->w0 & 0xFFFFFF );
    q->pC->w0 = ( q->pC->w0 & 0xFF000000 ) | ( (unsigned int)p & 0xFFFFFF );
}
