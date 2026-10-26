import sys,iso,toc,slz,codescan,struct,collections
disc=sys.argv[1]; p=f"roms/disc{disc}/Star Ocean - The Second Story (USA) (Disc {disc}).bin"
D=iso.Disc(p); w,sz=toc.load(D)
known={0x8002f810}
for i in range(1,0x1200):
    l,n=toc.ent(w,sz,i)
    if n==0: continue
    b=D.read(l,n*2048); kind='raw'
    if b[:3]==b'SLZ':
        try: b,_=slz.dec(b); kind='slz'
        except Exception: continue
    if len(b)<512: continue
    d=codescan.density(b)
    ww=struct.unpack('<%dI'%(len(b)//4),b[:len(b)//4*4])
    nj=sum(1 for x in ww if x>>26==3); nr=sum(1 for x in ww if x==0x03e00008); npro=sum(1 for x in ww if x>>16==0x27bd and x&0x8000)
    if nr>=5 and npro>=5 and d>0.7:
        r=codescan.base_vote(b)
        print(i,l,n,kind,len(b),round(d,2),'jal',nj,'jr',nr,'pro',npro,[(hex(a),c) for a,c in r[0][:2]],flush=True)
