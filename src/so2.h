/* Shared declarations for the main overlay (entry 1). Names are address-derived; layouts are inferred from accesses only. */
typedef struct Node { int pad0; struct Node *next; } Node;
typedef struct { char pad[2]; unsigned char b2; char pad3[0xD]; int w10; } Ctx;

extern unsigned short *D_8004DDA4;
extern void *D_8004DDB8;
extern Ctx *D_8004DDCC;
extern void *D_8004B5A8, *D_8004B838, *D_8004B898;
extern short D_8004EFBC;
extern void *D_8004EFC4, *D_8004EBE0;

void func_8001FD70();
void func_80023F40();
void func_80030FC8();
void func_80031008();
void func_80031CC8();
void func_80031E94();
void func_8006AB68();
void *func_80012108();
int func_8003469C();
int func_80034650();
int func_80034EF8();
int func_8003B8C4();
void func_8003B638();
void func_8003B83C();
void func_8003B5DC();
void func_80038760();
void func_80038854();
void func_8003867C();
void func_80035AA4();
void func_800367B4();
