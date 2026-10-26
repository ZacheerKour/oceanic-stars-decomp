#include "so2.h"
void *func_8003504C()
{
	int saved;
	void *p;

	saved = func_800340B0();
	do {
		p = func_800104D4(8);
		func_800340C0(p);
	} while ( !func_8003469C() || ( D_8004DDCC->b2 & 7 ) );
	func_800340C0(saved);
	func_80023F40(&D_8004B5C8, p);
	return p;
}
