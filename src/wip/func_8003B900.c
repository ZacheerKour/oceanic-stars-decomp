/* wall: loop, retail is not rotated (top test + j back); gcc rotates it: 12 bytes differ (same class as 800361CC) */
#include "so2.h"
int func_8003B900(unsigned short *p) {
    int i, r = 0;

    for ( i = 0; i < 1024; i++ ) {
        if ( (*p++ >> 10) & 0x1F ) r = i;
    }
    return r;
}
