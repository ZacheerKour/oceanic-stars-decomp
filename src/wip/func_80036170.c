/* DIFF 31 bytes: loop shape (retail non-rotated top test, found-branch falls through, continue path j-to-top out of line; ours beq to found block). Wall class CFG (same family as 800361CC). Tried for(;;)+early returns, while+break+result var. */
#include "so2.h"
int func_80036170(S_80036170 *o, int key, int add)
{
	int i;
	S_80036170e *e;

	i = 0;
	for ( ; ; ) {
		if ( i >= o->w0C ) return 0;
		e = &o->p8[i];
		if ( e->w0 == key ) {
			e->w4 += add;
			return 1;
		}
		i++;
	}
}
