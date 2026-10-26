/* wall: register residue, 65 differing bytes: retail keeps the slot pointer in a0 and f in v0 in the tail (store in beqz delay slot); ours uses v1/a1; tried pointer local vs repeated D_8004EFC4[i] */
#include "so2.h"
void func_80038760(int a) {
    int i;
    S_80038DBC *e;
    unsigned short f;

    if ( !a ) return;
    i = a - 1;
    e = D_8004EFC4 + i;
    f = e->h0;
    if ( !( f & 1 ) ) return;
    if ( f & 4 ) {
        e->h0 = f | 0x20;
        func_80011B48( D_8004EBE4 );
        if ( D_8004EFB8 ) {
            do {
                func_80038E10();
            } while ( D_8004EFB8 );
        }
    }
    f = D_8004EFC4[i].h0;
    D_8004EFC4[i].h0 = f & ~1;
    if ( f & 8 ) {
        func_8001FD70( D_8004EFC4[i].w4 );
        D_8004EFC4[i].w4 = 0;
    }
}
