for f in "$@"; do mipsel-linux-gnu-objdump -d -j .text build/obj/${GCC:-gcc-2.7.2-psx}/$f.elf | tail -n +8; done
