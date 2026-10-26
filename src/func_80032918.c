typedef struct { int w[8]; } Ent;
extern Ent D_8004ECD0[], D_8004ED90[], D_8004EE50[];
typedef struct { char pad[0x104]; Ent *a; Ent *b; Ent *c; } Obj;
void func_80032918(Obj *o, int i) { o->a = &D_8004ECD0[i]; o->b = &D_8004ED90[i]; o->c = &D_8004EE50[i]; }
