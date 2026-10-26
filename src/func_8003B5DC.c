#include "so2.h"
int func_8003B5DC( S_8003B5DC *o, int a1, int a2 ) {
    S_8003B5DCe *e;

    if ( a1 == 0 ) return 0;
    a1 = a1 - 1;
    e = o->p820 + a1;
    if ( a2 == 0 ) return e->w0;
    if ( (unsigned int)( a2 - 4 ) < 15 ) return *(short *)( (unsigned char *)e + a2 );
    return ( (unsigned char *)e )[a2];
}
