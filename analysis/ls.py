import sys,iso
for p in sys.argv[1:]:
    d=iso.Disc(p); v,fs=d.walk(); print(v,d.n,'sectors')
    for f in sorted(fs): print(f)
