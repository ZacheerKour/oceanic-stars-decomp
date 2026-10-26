typedef struct { char pad[0xD4]; int p; char pad2[8]; int q; } Obj;
void func_80032308(Obj *o, int *x, int *y) { *x = o->p; *y = o->q; }
