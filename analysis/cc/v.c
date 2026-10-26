extern int tab[64]; extern short stab[64]; extern unsigned char btab[256]; extern void (*fp)(int); extern int g1,g2,g3; extern void ext(int,int);
int a1(int x,int y){ return (x>y)?x:y; }
int a2(int x){ return x&&g1||g2; }
int a3(int *p,int n){ int i; for(i=0;i<n;i++){ if(p[i]<0) p[i]=-p[i]; } return n; }
void a4(unsigned char *d,unsigned char *s,int n){ while(n--) *d++=*s++; }
int a5(int x){ return tab[x]+stab[x]+btab[x]; }
void a6(int x){ g1=x; g2=x+1; g3=x+2; fp(x); }
int a7(int a,int b,int c,int d,int e){ ext(a,b); ext(c,d); return e+a; }
int a8(int x){ switch(x){case 0:return 3;case 1:return 8;case 2:g1=1;return 9;case 3:return g2;case 4:return 11;case 5:return 12;case 6:return 13;case 7:ext(1,1);return 14;default:return -1;} }
unsigned a9(unsigned x){ return (x<<3)|(x>>29); }
int b1(int x){ return x/7+x%16-x/4; }
int b2(short x,short y){ return x*y>>8; }
long long b3(long long a,long long b){ return a+b; }
int b4(int *p){ int s=0; while(*p){ s+=*p++; if(s>100) break; } return s; }
void b5(void){ int i; for(i=0;i<16;i++){ tab[i]=i*i; btab[i]=i; stab[i]=-i; } }
int b6(char *s){ int n=0; while(*s++) n++; return n; }
