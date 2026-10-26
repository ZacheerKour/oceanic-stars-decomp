#include "so2.h"
int func_80035364(int a) {
	int saved, found, i, r;

	saved = func_800340B0();
	found = 0;
	i = 0;
	while (1) {
		if (i >= 4) break;
		func_800340C0(i);
		r = func_8003469C();
		if (r && D_8004DDD0->b3B == a) found = r;
		i++;
	}
	func_800340C0(saved);
	return found;
}
