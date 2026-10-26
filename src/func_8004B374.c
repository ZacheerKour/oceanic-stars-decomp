#include "so2.h"
int func_8004B374( signed char *a, signed char *b )
{
    int r;

    if ( a == 0 || b == 0 ) {
        if ( a == b ) {
            r = 0;
        } else {
            r = -1;
            if ( a != 0 ) {
                r = 1;
            }
        }
    } else {
        while ( *a == *b++ ) {
            if ( (unsigned char)*a++ == 0 ) {
                return 0;
            }
        }
        r = *a - b[-1];
    }
    return r;
}
