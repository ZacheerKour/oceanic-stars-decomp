/* DIFF 90+ bytes: loop shape (retail non-rotated: top test slti/beqz, body, j top with i++ in delay; ours rotated). Wall class CFG, same family as 800361CC/800386FC. Tried while, for(;;)+break (35364). */
#include "so2.h"
int func_80030E1C(S_80030E1C *o)
{
	int saved;
	int i;

	o->b1B = 1;
	saved = func_800340B0();
	for ( i = 0; i < 8; i++ ) {
		func_800340C0(i);
		if ( func_8003469C() && !( D_8004DDCC->b2 & 7 ) ) o->b1B = 0;
	}
	func_800340C0(saved);
	return o->b1B;
}
