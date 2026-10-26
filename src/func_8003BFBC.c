#include "so2.h"
int func_8003BFBC(int a) {
    int i;

    for ( i = 0; i < 12; i++ ) {
        if ( !D_8004E0D8[i][0] ) {
            D_8004E0D8[i][0] = func_8001FE30( a * 4 );
            D_8004E0D8[i][1] = func_8001FE30( a * 4 );
            D_8004E138[i] = a;
            return i;
        }
    }
    return -1;
}
