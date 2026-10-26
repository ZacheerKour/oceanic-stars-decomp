/* DIFF 10 bytes: register (retail base in v1, offset in v0; ours swapped). Wall: register. */
#include "so2.h"
int func_80034110(int a) {
	int off = 0, k = a & 0x1F, f, base;
	func_800342D8(&off, k);
	f = (a & 0xF00) >> 8;
	if (D_8004B5D0[k] & 2) {
		int *p;
		if (D_8004B5D0[k] & 1) base = (int)D_8004DDD0; else base = (int)D_8004DDCC;
		p = (int *)(off + base);
		return p[f];
	} else {
		short *p;
		if (D_8004B5D0[k] & 1) base = (int)D_8004DDD0; else base = (int)D_8004DDCC;
		p = (short *)(off + base);
		return p[f];
	}
}
