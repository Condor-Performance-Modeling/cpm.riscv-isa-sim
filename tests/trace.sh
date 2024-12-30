#! /bin/bash
# ------------------------------------------------------------------
# Sample isa string from spike execution
#xrv64i2p1_m2p0_a2p1_f2p2_d2p2_c2p0_zicsr2p0_zifencei2p0_zmmul1p0_zba1p0_zbb1p0_zbc1p0_zbs1p0
# ------------------------------------------------------------------
# Trace start/stop encoded values
# START_TRACE 00004033
# STOP_TRACE  0010c033
# ------------------------------------------------------------------
# Tool paths
# ------------------------------------------------------------------
OBJ_DUMP=$TOP/riscv64-unknown-elf/bin/riscv64-unknown-elf-objdump
STF_DUMP=/data/tools/bin/stf_record_dump
SPIKE=../build/spike
DROM=../../cpm.dromajo/build/dromajo
# ------------------------------------------------------------------
# Tool opts
# ------------------------------------------------------------------
DOPTS="--disassemble-all --disassemble-zeroes --section=.text --section=.text.startup --section=.text.init  --section=.data -Mnumeric,no-aliases"
ISA=rv64imafdc_zicntr_zihpm_zba_zbb_zbc_zbs
STF="--stf_macro_tracing --stf_priv_modes USHM --stf_force_zero_sha"
DSTF="--stf_priv_modes USHM --stf_force_zero_sha"
# ------------------------------------------------------------------
# bmi_sanity has 7 regions (start/stop pairs) and an extra stop at the end
# ------------------------------------------------------------------
ELFS=(
"bmi_pmp.bare.riscv"
"bmi_sanity.bare.riscv"
"coremark_10.bare.riscv"
"dhrystone_opt1.10.bare.riscv"
"dhrystone_opt2.10.bare.riscv"
"dhrystone_opt3.10.bare.riscv"
)

ELF=${ELFS[1]}

# ------------------------------------------------------------------
mkdir -p drom_out
mkdir -p spike_out

rm -f drom_out/*
rm -f spike_out/*
# ------------------------------------------------------------------
$OBJ_DUMP $DOPTS elfs/$ELF > $ELF.objdump

##$SPIKE --isa=$ISA elfs/$ELF
##$SPIKE --isa=$ISA -d elfs/$ELF
$SPIKE --isa=$ISA --stf_trace spike_out/$ELF.zstf $STF elfs/$ELF
$STF_DUMP spike_out/$ELF.zstf > spike_out/$ELF.stf_dump

echo "\n"
##$DROM --march=$ISA elfs/$ELF
##$DROM --march=$ISA -d elfs/$ELF
$DROM --march=$ISA $DSTF --stf_trace drom_out/$ELF.zstf elfs/$ELF
$STF_DUMP drom_out/$ELF.zstf > drom_out/$ELF.stf_dump
