CC=/data/tools/riscv-embecosm-embedded-ubuntu2204-20240407-14.0.1

#$CC/bin/riscv64-unknown-elf-gcc \
#-march=rv64gc_zba_zbb_zbc_zbs \
#-mabi=lp64d \
#-nostdlib \
#-nostartfiles \
#-mcmodel=medany \
#-static \
#-std=gnu99 \
#-Tsrc/linker.ld \
#src/simple.S \
#-o ./elfs/minimal.bare.riscv \
#-lm -lgcc

$CC/bin/riscv64-unknown-elf-gcc \
-march=rv64gc_zba_zbb_zbc_zbs \
-mabi=lp64d \
-nostdlib \
-nostartfiles \
-mcmodel=medany \
-O3 \
-ffast-math \
-funsafe-math-optimizations \
-finline-functions \
-fno-common \
-fno-builtin-printf \
-flto \
-fno-tree-loop-distribute-patterns \
-D__riscv=1 \
-static \
-std=gnu99 \
-Tsrc/linker.ld \
src/simple.c \
src/crt.S \
src/syscalls.c \
-o ./elfs/simple.rv64gc_with_zbx.bare.riscv \
-lm -lgcc


