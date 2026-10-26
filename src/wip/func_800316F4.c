/* DIFF ~14 insns shifted: retail early "return 1" is "bne q,p,L; (a4=0 in delay); j end; li v0,1" and tail "beqz n,end; (li -1 in delay)"; ours folds to "beq q,p,end" and "bnez n; nop; j". Wall class control-flow/schedule (branch layout). Tried single-r if/else, nested ifs, r=-1 default, split 8833 ifs. */
#include "so2.h"
int func_800316F4(S_800316F4 *o)
{
    S_800316F4s *p;
    S_800316F4s *q;
    int n;
    int f;
    int busy;
    int r;

    busy = 0;
    p = &o->s[0];
    q = &o->s[1];
    if ( p->h782 == 0x1141 ) {
        if ( q->h782 == p->h782 ) {
            return 1;
        }
    }
    n = 0;
    f = 0;
    if ( p->h782 == 0x2282 ) {
        busy = 1;
        if ( p->h78C == 0 ) {
            n = 1;
            f = p->h78E != 0;
        }
    }
    if ( q->h782 == 0x2282 ) {
        busy = 1;
        if ( q->h78C == 0 ) {
            n++;
            if ( q->h78E != 0 ) {
                f = 1;
            }
        }
    }
    if ( busy ) {
        return 0;
    }
    if ( p->h782 == 0x2281 ) {
        return 0;
    }
    if ( q->h782 == 0x2281 ) {
        return 0;
    }
    if ( n == 0 ) {
        return -1;
    }
    r = n + 1;
    if ( f ) {
        r |= 4;
    }
    return r;
}
