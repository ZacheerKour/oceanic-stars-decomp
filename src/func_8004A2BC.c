#include "so2.h"
int func_8004A2BC(int a0, unsigned char *p) {
    unsigned char *t = D_8004EC20->b8;
    int n = 0;
    unsigned short c;
    while ( 1 ) {
        c = *p++;
        if ( c == 0 ) {
            break;
        }
        if ( c >= 128 ) {
            c = ( c & 0x7F ) | ( *p++ << 7 );
        }
        c--;
        n += t[c];
    }
    return n;
}
