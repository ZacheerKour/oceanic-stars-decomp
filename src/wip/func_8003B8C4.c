/* wall: loop, retail is not rotated (top test + j back); gcc rotates it: 12 bytes differ (same class as 800361CC) */
#include "so2.h"
int func_8003B8C4(unsigned short *p, int v) {
    int i;

    for ( i = 0; i < 1024; i++ ) {
        if ( (*p++ & 0x3FF) == v ) return i;
    }
    return -1;
}
