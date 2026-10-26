#include "so2.h"
void func_80031194(int a) {
	int i;

	for (i = 0; i < 16; i++) {
		if (D_8004DDD4[i].p0 && D_8004DDD4[i].w4 == a) {
			func_80013254(D_8004DDD4[i].p0, i, a);
			D_8004DDD4[i].p0 = 0;
			return;
		}
	}
}
