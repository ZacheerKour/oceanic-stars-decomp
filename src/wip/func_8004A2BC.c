/* 80 differing bytes: loop rotated vs retail top-test (wall: CFG). compiler hoists/drops the ushort andi copy */
#include "so2.h"
int func_8004A2BC(int a0, unsigned char *p) {
    unsigned char *t = D_8004EC20->b8;
    int n = 0;
    unsigned short c;
    for ( ;; ) {
        c = *p++;
        if ( c == 0 ) {
            break;
        }
        if ( c >= 128 ) {
            c = ( c & 0x7F ) | ( *p++ << 7 );
        }
        n += t[(unsigned short)( c - 1 )];
    }
    return n;
}
