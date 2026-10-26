#include "so2.h"
unsigned short func_8003CB98(int a, int b, int c) {
    char *buf;
    int r;
    buf = func_80012154(D_8004EBE8, a, b);
    func_800121A8(D_8004EBE8, a, buf, b);
    r = func_80012EC4(buf + 20, 0, c);
    func_80020E5C(0);
    func_8001FD70(buf);
    return r;
}
