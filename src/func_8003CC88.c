#include "so2.h"
void func_8003CC88(S_8003CC88 *o, int flags) {
    func_8003C07C(o->p0->w4);
    func_8003C0E0(o->p0->w8);
    if ( o->p4 ) {
        func_8001FD70(o->p4);
    }
    func_8001FD70(o->p0);
    func_80020DC4(0);
    func_80020AE0(1);
    func_8003BAF0();
    func_8003BAF0();
    if ( flags & 1 ) {
        func_8001FD70(o);
    }
}
