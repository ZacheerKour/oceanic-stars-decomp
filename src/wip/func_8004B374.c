/* 18 differing bytes: loop body matches; null-arg prologue gets a spurious sp frame and different branch layout (wall: CFG/frame) */
#include "so2.h"
int func_8004B374(signed char *a, signed char *b) {
    if ( a == 0 || b == 0 ) {
        return a == b ? 0 : a == 0 ? -1 : 1;
    }
    while ( *a == *b++ ) {
        if ( (unsigned char)*a++ == 0 ) {
            return 0;
        }
    }
    return *a - b[-1];
}
