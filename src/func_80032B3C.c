#include "so2.h"
void func_80032B3C(S_80030CBC *o, int a, int n) {
	char buf[32];
	char out[32];
	int base, i, sum, r;
	unsigned char d;
	unsigned char c;

	base = o->wE0 + ((S_80033FD0p *)o->wE0)->w0;
	func_800100EC(buf, a, n, 32);
	out[0] = 0;
	sum = 0;
	i = 0;
	while (1) {
		if (i >= n + 1) break;
		c = buf[i];
		if (c == 0) break;
		if (c != 32) {
			if (c == 45) sum += (short)func_80032D48(o, out, base, 12);
			else {
				d = c - 48;
				sum += (short)func_80032D48(o, out, base, d);
			}
		}
		i++;
	}
	r = func_80032984(o);
	r = n * 6 + r - sum;
	if (r < 0) r = 0;
	func_80032950(o, r);
	func_800339F8(o, out);
}
