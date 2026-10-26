/* DIFF 90+ bytes: loop shape (retail non-rotated: top test slti/beqz, body, j top with i++ in delay; ours rotated). Wall class CFG, same family as 800361CC/800386FC. Tried while, for(;;)+break (35364). */
#include "so2.h"
int func_80035364(int a)
{
	int saved;
	int found;
	int i;
	int r;

	saved = func_800340B0();
	found = 0;
	for ( i = 0; i < 4; i++ ) {
		func_800340C0(i);
		r = func_8003469C();
		if ( r && D_8004DDD0[59] == a ) found = r;
	}
	func_800340C0(saved);
	return found;
}
