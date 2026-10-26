#include "so2.h"
int func_8003B8C4(unsigned short *p, int v) {
    int i;

    i = 0;
    while ( 1 ) {
        if ( i >= 1024 ) break;
        if ( ( *p++ & 0x3FF ) == v ) return i;
        i++;
    }
    return -1;
}
