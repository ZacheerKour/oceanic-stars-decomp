#include "so2.h"
int func_8004A320(int a0, unsigned char *a1)
{
    unsigned short c;
    int n;

    n = 0;
    while ( 1 ) {
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
