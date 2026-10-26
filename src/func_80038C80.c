#include "so2.h"
int func_80038C80(int a) {
    int i;

    for ( i = 0; i < D_8004EFBC; i++ ) {
        if ( D_8004EFC4[i].w10 == a ) {
            if ( !func_80038D14( a, i + 1 ) ) return 0;
        }
    }
    return 1;
}
