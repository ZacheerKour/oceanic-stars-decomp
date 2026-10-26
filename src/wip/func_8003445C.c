/* DIFF 13 bytes: register (retail b in v0, offset-table index in v1; ours swapped). Wall: register. D_8004DE94 as byte-base int. */
#include "so2.h"
int func_8003445C(short i) { int b = D_8004DE94, x = b + ((int *)b)[D_8004DE98]; return x + ((int *)x)[i]; }
