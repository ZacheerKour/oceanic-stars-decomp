#include "so2.h"
void func_8003C3D8(void) {
    int i, j;

    j = 1 - D_8004DED4;
    D_8004DED4 = j;
    for ( i = 0; i < 12; i++ ) {
        D_8004E078[i][j] = D_8004DFB8[i][j];
    }
}
