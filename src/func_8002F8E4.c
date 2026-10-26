#include "so2.h"
int func_8002F8E4(int a) {
	if (a <= 1024) return D_8004D58C[a];
	if (a < 2048) return D_8004D58C[2048 - a];
	if (a <= 3072) return -D_8004D58C[a - 2048];
	return -D_8004D58C[4096 - a];
}
