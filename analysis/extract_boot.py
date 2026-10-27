"""Write the boot executable SCUS_944.21 from a disc 1 .bin image. Usage: extract_boot.py <disc1.bin> <out file>"""
import sys
sys.path.insert(0, 'analysis')
import iso
d = iso.Disc(sys.argv[1])
f = {n.upper(): (l, s) for n, l, s, fl in d.walk()[1]}
l, s = f['/SCUS_944.21;1']
open(sys.argv[2], 'wb').write(d.read(l, s))
print('boot executable written')
