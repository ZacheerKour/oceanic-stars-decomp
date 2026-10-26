/* MATCH-PENDING: byte-exact (184/184) but R4 FAIL: (unsigned char *)o + 15 byte-pointer cast. Without it 2 bytes differ (retail pointer = o+15 with sb 32(p)). Outer loop uses while(1)+break trick. */
#include "so2.h"
void func_80030D64(S_80030D64 *o) {
	int i, j, n;
	unsigned char *p;

	n = 15;
	p = (unsigned char *)o + 15;
	for (; n >= 0; n--) { p[32] = n; p--; }
	i = 0;
	while (1) {
		if (i >= 8) break;
		for (j = 0; j < 8; j++) {
			if ((unsigned short)func_800349D8(i) == D_8004B410[j]) o->b20[i] = j;
		}
		i++;
	}
}
