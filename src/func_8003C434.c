#include "so2.h"
S_8003EC54_tag *func_8003C434(int i, unsigned n) {
	unsigned cur = D_8004E078[i][D_8004DED4];
	if (D_8004E018[i][D_8004DED4] < cur + n) {
		if (D_8004E170) D_8004E170();
		return 0;
	}
	D_8004E078[i][D_8004DED4] = cur + n;
	return (S_8003EC54_tag *)cur;
}
