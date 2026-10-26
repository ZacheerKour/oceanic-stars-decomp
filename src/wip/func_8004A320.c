/* 42 differing bytes: loop rotated (ours jumps to bottom test) vs retail top-test with j back. wall: CFG, same class as 8004A2BC */
#include "so2.h"
int func_8004A320(int a0, unsigned char *a1)
{
    unsigned short c;
    int n = 0;

    for ( ;; ) {
        c = *a1++;
        if ( c == 0 ) {
            break;
        }
        if ( c >= 128 ) {
            a1++;
        }
        n++;
    }
    return n;
}
