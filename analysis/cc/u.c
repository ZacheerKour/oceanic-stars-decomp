struct S { int a,b,c,d; short e; char f; };
extern struct S gs, gt; extern int tab[64]; extern void ext(int,int);
int d10(int x){ return x/10 + x%10; }
int d75(unsigned x){ return x/75; }
int m3(int x){ return x*3 + x*40 + x*100; }
void cp(void){ gs = gt; }
int sw(int x){ switch(x){case 0: return 5; case 1: return 9; case 2: return 14; case 3: ext(1,2); return 1; case 4: return 77; case 5: return tab[3]; default: return 0;} }
int lp(int *p,int n){ int s=0,i; for(i=0;i<n;i++) s+=p[i]*2; return s; }
int fl(struct S *s,int k){ if(s->e>k && s->f){ s->a++; ext(s->b,k); } else s->c=k; return s->d; }
short sh(short *p){ return (p[0]+p[1])>>1; }
