import sys,iso,toc,slz,codescan,struct
disc=sys.argv[1]; p=f"roms/disc{disc}/Star Ocean - The Second Story (USA) (Disc {disc}).bin"
D=iso.Disc(p); w,sz=toc.load(D)
for i in range(1,0x1200):
    l,n=toc.ent(w,sz,i)
    if n==0: continue
    b=D.read(l,n*2048)
    if b[:3]!=b'SLZ': continue
    off=0;k=0
    while True:
        o,nx=slz.dec(b,off)
        if len(o)>=256:
            nj=sum(1 for j in range(0,len(o)-3,4) if o[j+3]>>2==3)
            r=codescan.base_vote(o) if nj>=10 else None
            if r and r[0] and r[0][0][1]>=0.3*r[1] and r[1]>=10:
                print(i,l,n,k,'v%d'%b[off+3],len(o),hex(r[0][0][0]),r[0][0][1],r[1],r[2],flush=True)
        if not nx: break
        off+=nx;k+=1
