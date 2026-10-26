import sys,iso,toc,slz,codescan,struct
disc=sys.argv[1]; p=f"roms/disc{disc}/Star Ocean - The Second Story (USA) (Disc {disc}).bin"
D=iso.Disc(p); w,sz=toc.load(D)
rows=[]
for i in range(1,0x1200):
    l,n=toc.ent(w,sz,i)
    if n==0: continue
    b=D.read(l,n*2048)
    if b[:3]!=b'SLZ': continue
    try: o,nx=slz.dec(b)
    except Exception as e: rows.append((i,l,n,'ERR',0,0,0,'')); continue
    hdr=struct.unpack_from('<II',b,4)
    dn=codescan.density(o) if len(o)>=64 else 0
    rows.append((i,l,n,b[3],hdr[0],len(o),round(dn,2),nx))
    print(*rows[-1],flush=True)
