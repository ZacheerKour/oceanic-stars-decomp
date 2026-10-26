#include "so2.h"
void func_80038BC4(int a, int b) {
    int i, n;

    i = 0;
    while ( 1 ) {
        if ( i >= D_8004EFBC ) break;
        n = i + 1;
        if ( D_8004EFC4[i].h0 & 1 ) {
            if ( D_8004EFC4[i].w10 == b ) func_80038760( n );
        }
        i++;
    }
}
