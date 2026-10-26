#include "so2.h"
void func_8003BA74() {
    unsigned char *p;
    int i;

    D_8004DED4 = 1;
    D_8004E170 = 0;
    i = 47;
    p = D_8004E234 + 47;
    for ( ; i >= 0; i-- ) *p-- = 0;
    for ( i = 0; i < 12; i++ ) {
        D_8004DFB8[i][0] = 0;
        D_8004DFB8[i][1] = 0;
        D_8004E0D8[i][0] = 0;
        D_8004E0D8[i][1] = 0;
    }
}
