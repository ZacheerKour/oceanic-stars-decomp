#include "so2.h"
void func_80038760(int a) {
    int i;
    unsigned short f;

    if ( !a ) return;
    i = a - 1;
    f = D_8004EFC4[i].h0;
    if ( !( f & 1 ) ) return;
    if ( f & 4 ) {
        D_8004EFC4[i].h0 = f | 0x20;
        func_80011B48( D_8004EBE4 );
        if ( D_8004EFB8 ) {
            do {
                func_80038E10();
            } while ( D_8004EFB8 );
        }
    }
    D_8004EFC4[i].h0 &= ~1;
    if ( D_8004EFC4[i].h0 & 8 ) {
        func_8001FD70( D_8004EFC4[i].w4 );
        D_8004EFC4[i].w4 = 0;
    }
}
