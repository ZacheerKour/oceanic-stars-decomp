/* wall: frame size only (56 vs 64, vars 16 vs 24; everything else byte-identical modulo sp offsets). Each strength-reduced 20-byte-stride loop reserves 8 bytes of vars; retail has three such loops, we write two (slot search, flag clear); a dead third loop adds the slot but also code. Semantics: find free slot (h0&1==0) in D_8004EFC4 table of D_8004EFBC entries (20 bytes); if none grow by 10, clear flags, copy old, free old; mark p used (h0|=1, w10=a), copy *p into slot, return slot+1. */
#include "so2.h"
int func_80038A1C(int a, S_80038DBC *p) {
    int i, j, slot, n;
    S_80038DBC *old;

    slot = -1;
    for ( i = 0; i < D_8004EFBC; i++ ) {
        if ( !( D_8004EFC4[i].h0 & 1 ) ) {
            slot = i;
            break;
        }
    }
    if ( slot == -1 ) {
        n = D_8004EFBC;
        old = D_8004EFC4;
        D_8004EFBC = n + 10;
        D_8004EFC4 = func_8001FE30( D_8004EFBC * 20 );
        for ( j = 0; j < D_8004EFBC; j++ ) D_8004EFC4[j].h0 = 0;
        if ( n ) func_80023ED0( D_8004EFC4, old, n * 20 );
        func_8001FD70( old );
        slot = n;
    }
    p->h0 |= 1;
    p->w10 = a;
    D_8004EFC4[slot] = *p;
    return slot + 1;
}
