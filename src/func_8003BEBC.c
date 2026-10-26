#include "so2.h"
int func_8003BEBC(int a) {
    int i;

    for ( i = 0; i < 12; i++ ) {
        if ( !D_8004DFB8[i][0] ) {
            D_8004DFB8[i][0] = func_8001FE30( a );
            D_8004DFB8[i][1] = func_8001FE30( a );
            D_8004E018[i][0] = D_8004DFB8[i][0] + a;
            D_8004E018[i][1] = D_8004DFB8[i][1] + a;
            D_8004E078[i][D_8004DED4] = D_8004DFB8[i][D_8004DED4];
            return i;
        }
    }
    return -1;
}
