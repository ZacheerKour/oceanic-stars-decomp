#include "so2.h"
void func_80048F34()
{
    int i;

    D_8004ECD0[0].h0 = 0;
    D_8004EC6C = 0;
    for ( i = 0; i < 16; i++ ) {
        D_8004EC70[i] = 0;
        D_8004ECB0[i] = 0;
    }
}
