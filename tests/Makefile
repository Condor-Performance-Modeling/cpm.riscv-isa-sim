.PHONY: defaul

BASE=/data/tools/riscv-embecosm-embedded-ubuntu2204-20240407-14.0.1
CC=$(BASE)/bin/riscv64-unknown-elf-gcc

OPT  = -O3
STD  = -std=gnu99 -nostdlib -nostartfiles -static
MARCH =rv64gc_zba_zbb_zbc_zbs
MABI = -mabi=lp64d -mcmodel=medany
DEF  = -D__riscv=1
FUNC = -ffast-math -funsafe-math-optimizations -finline-functions \
       -fno-common -fno-builtin-printf -flto \
       -fno-tree-loop-distribute-patterns 
LNK  = -T./src/linker.ld
LIBS = -lm -lgcc

CFLAGS = $(OPT) $(STD) $(MABI) $(DEF) $(FUNC) $(LNK)

CMN=src/crt.S src/syscalls.c

A0 =rv64gc
A1 =rv64gc_zba_zbb_zbc_zbs

default:
	$(MAKE) ./elfs/simple.$(A0).bare.riscv
	$(MAKE) ./elfs/simple.$(A1).bare.riscv
	$(MAKE) ./elfs/zbx.$(A1).bare.riscv

./elfs/simple.$(A0).bare.riscv: src/simple.c $(CMN)
	-mkdir -p elfs
	$(CC) -march=$(A0) $(CFLAGS) $^ -o $@ $(LIBS)

./elfs/simple.$(A1).bare.riscv: src/simple.c $(CMN)
	-mkdir -p elfs
	$(CC) -march=$(A1) $(CFLAGS) $^ -o $@ $(LIBS)

./elfs/zbx.$(A1).bare.riscv: src/zbx.c $(CMN)
	-mkdir -p elfs
	$(CC) -march=$(A1) $(CFLAGS) $^ -o $@ $(LIBS)

clean:
	-rm -f elfs/*
