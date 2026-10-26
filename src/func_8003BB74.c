#include "so2.h"
void func_8003BB74(int a) {
    int i;

    for ( i = 0; i < 48; i++ ) {
        if ( D_8004E234[i] == 0 ) {
            D_8004E234[i] = 2;
            D_8004E174[i] = a;
            break;
        }
    }
}
