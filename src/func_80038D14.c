#include "so2.h"
int func_80038D14(int a, int b) {
    int i, j;
    unsigned short f, g;

    if ( b == -1 ) {
        for ( i = 0; i < D_8004EFBC; i++ ) {
            f = D_8004EFC4[i].h0;
            if ( ( f & 1 ) && ( f & 2 ) ) return 0;
        }
        return 1;
    }
    j = b - 1;
    g = D_8004EFC4[j].h0;
    if ( g & 1 ) {
        if ( g & 2 ) return 0;
    }
    return 1;
}
