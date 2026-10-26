/* wall: 97 bytes. Retail keeps "move s2,zero" (f=0) in the delay slot of the beqz after the first flag test (it is the first insn of the else block, hoisted by reorg); our f=0 there is CSE-deleted, so 224B vs 228B. Semantics right: v==0->0; f=(165|220|221) -> v>>=1 else f=(161|210) -> v=v*2/3; result max(v,1). Tried reset in then, in else, !f test, goto. */
#include "so2.h"
int func_8003B47C( void *o, int v ) {
    int f;

    if ( v == 0 ) return 0;
    f = 0;
    if ( func_8003B560( o, 165 ) || func_8003B560( o, 220 ) || func_8003B560( o, 221 ) ) f = 1;
    if ( f ) {
        v >>= 1;
    } else {
        f = 0;
        if ( func_8003B560( o, 161 ) || func_8003B560( o, 210 ) ) f = 1;
        if ( f ) v = v * 2 / 3;
    }
    if ( v == 0 ) return 1;
    return v;
}
