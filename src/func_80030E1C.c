#include "so2.h"
int func_80030E1C(S_80030E1C *o) {
	int saved, i;

	o->b1B = 1;
	saved = func_800340B0();
	i = 0;
	while (1) {
		if (i >= 8) break;
		func_800340C0(i);
		if (func_8003469C() && !(D_8004DDCC->b2 & 7)) o->b1B = 0;
		i++;
	}
	func_800340C0(saved);
	return o->b1B;
}
