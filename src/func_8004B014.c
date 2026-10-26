#include "so2.h"
char *func_8004B014(char *a0, char *a1)
{
    char *r;

    if ( a0 == 0 || a1 == 0 ) {
        return 0;
    }
    r = a0;
    while ( (*a0++ = *a1++) != 0 ) {
    }
    return r;
}
