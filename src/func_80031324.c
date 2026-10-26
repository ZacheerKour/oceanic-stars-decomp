#include "so2.h"
void func_80031324(S_80031290 *o, int flags) {
	int i = 0, k;

	while (1) {
		if (i >= 2) break;
		k = i * 12 + 4048;
		func_8001FD70(((S_80031290e *)((int)o + k))->p0);
		i++;
	}
	func_80036ACC(o, 2);
	if (flags & 1) func_8001FD70(o);
}
