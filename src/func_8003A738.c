#include "so2.h"
void func_8003A738() {
	int i = 0, r;
	while (1) {
		if (i >= 7) break;
		r = func_80034B6C(i);
		if (r) {
			D_8004DDCC->h4C += func_80034FF8(r, 44);
			D_8004DDD0->hA += func_80034FF8(r, 20);
			D_8004DDD0->h4 += func_80034FF8(r, 22);
		}
		i++;
	}
	if (D_8004DDCC->h4C >= 256) D_8004DDCC->h4C = 255;
	if (D_8004DDD0->hA >= 10000) D_8004DDD0->hA = 9999;
	if (D_8004DDD0->h4 >= 256) D_8004DDD0->h4 = 255;
}
