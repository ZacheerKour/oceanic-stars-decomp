#include "so2.h"
int func_8003B900(unsigned short *p) {
    int i, r;

    r = 0;
    i = 0;
    while ( 1 ) {
        if ( i >= 1024 ) break;
        if ( ( *p >> 10 ) & 0x1F ) r = i;
        p++;
        i++;
    }
    return r;
}
