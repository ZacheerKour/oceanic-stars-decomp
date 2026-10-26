#include "so2.h"
void func_8003BAF0() {
    int i;

    for ( i = 0; i < 48; i++ ) {
        if ( D_8004E234[i] ) {
            D_8004E234[i]--;
            if ( D_8004E234[i] == 0 ) func_8001FD70( D_8004E174[i] );
        }
    }
}
