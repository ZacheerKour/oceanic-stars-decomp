import struct
def density(b):
    # fraction of words that are plausible MIPS: count of common opcodes patterns
    n=len(b)//4
    if n==0: return 0
    w=struct.unpack('<%dI'%n,b[:n*4]); c=0
    for x in w:
        op=x>>26
        if x==0 or op in(9,15,35,43,4,5,3,13,12,8,32,36,40,33,37,41,10,11,7,6) or (op==0 and (x&0x3f) in(33,37,8,0,2,3,35,43,36,39,42)) or x==0x03e00008: c+=1
    return c/n
def jal_base(b,lo=0x80010000,hi=0x80200000):
    n=len(b)//4; w=struct.unpack('<%dI'%n,b[:n*4])
    t=[(x&0x3ffffff)<<2|0x80000000 for x in w if x>>26==3]
    pro={0x80000000+i*4 for i,x in enumerate(w) if x>>16==0x27bd and x&0x8000}
    best=(0,0)
    for B in range(lo,hi,4):
        pass
    # candidate: align prologue offsets against jal target histogram
    offs=set(i*4 for i,x in enumerate(w) if x>>16==0x27bd and x&0x8000)
    from collections import Counter
    cnt=Counter(tg-o for tg in t for o in ()) 
    return t,offs
def best_base(b):
    t,offs=jal_base(b)
    if not t: return None
    from collections import Counter
    c=Counter()
    ts=Counter(t)
    # for each jal target, assume it hits a prologue offs -> base=tg-off; vote using the few smallest targets only
    tmin=min(t)
    for o in offs:
        B=tmin-o
        if B%4==0 and 0x80000000<B<0x80200000: c[B]+=0
    best=None
    for B in {tmin-o for o in offs if 0x80000000<tmin-o<0x80200000}:
        s=sum(1 for tg in ts if tg-B in offs)
        if best is None or s>best[1]: best=(B,s)
    return best,len(ts)

def base_vote(b):
    from collections import Counter
    n=len(b)//4; w=struct.unpack('<%dI'%n,b[:n*4])
    T=sorted({(x&0x3ffffff)<<2|0x80000000 for x in w if x>>26==3})
    F=[i*4 for i in range(n) if (x:=w[i])>>16==0x27bd and x&0x8000]
    c=Counter()
    for t in T:
        for f in F:
            B=t-f
            if 0x80010000<=B<=0x80200000: c[B]+=1
    return c.most_common(3),len(T),len(F)
