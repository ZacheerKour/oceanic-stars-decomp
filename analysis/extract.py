import os,iso,toc,slz,hashlib
out='analysis/out'
P={n:f"roms/disc{n}/Star Ocean - The Second Story (USA) (Disc {n}).bin" for n in(1,2)}
D={n:iso.Disc(P[n]) for n in P}; T={n:toc.load(D[n]) for n in P}
h1={}
os.makedirs(out+'/d1',exist_ok=True); os.makedirs(out+'/d2',exist_ok=True)
man=open(out+'/manifest.tsv','w'); man.write('disc\tidx\tlba\tsectors\tkind\tsize\tmd5raw\tsame_as_d1\n')
for n in(1,2):
    w,sz=T[n]
    for i in range(1,0x1200):
        l,c=toc.ent(w,sz,i)
        if c==0: continue
        b=D[n].read(l,c*2048); h=hashlib.md5(b).hexdigest()
        if n==1: h1[i]=h
        same=int(h1.get(i)==h) if n==2 else ''
        kind='slz%d'%b[3] if b[:3]==b'SLZ' else 'raw'
        if n==2 and same: man.write(f'{n}\t{i}\t{l}\t{c}\t{kind}\t-\t{h}\t1\n'); continue
        if kind[:3]=='slz':
            data,nx=slz.dec(b); ext='dec'
            if nx: kind+='+chain'
        else: data=b; ext='bin'
        open(f'{out}/d{n}/{i:04d}_{l}.{ext}','wb').write(data)
        man.write(f'{n}\t{i}\t{l}\t{c}\t{kind}\t{len(data)}\t{h}\t{same}\n')
