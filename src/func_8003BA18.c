#include "so2.h"
int func_8003BA18(void *o) {
    int i;

    for ( i = 0; i < 1024; i++ ) {
        if ( !func_8003B9CC( o, i ) ) return 0;
    }
    return 1;
}
