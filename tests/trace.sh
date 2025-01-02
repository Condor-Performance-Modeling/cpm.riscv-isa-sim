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
STF_RDUMP=/data/tools/bin/stf_record_dump
STF_DUMP=/data/tools/bin/stf_dump
SPIKE=../build/spike
DROM=../../cpm.dromajo/build/dromajo
# ------------------------------------------------------------------
# Tool opts
# ------------------------------------------------------------------
DOPTS="--disassemble-all --disassemble-zeroes --section=.text --section=.text.startup --section=.text.init  --section=.data -Mnumeric,no-aliases"
ISA=rv64imafdc_zicntr_zihpm_zba_zbb_zbc_zbs
STF="--stf_macro_tracing --stf_priv_modes USHM --stf_force_zero_sha"

DSTF="--march=rv64gc_zba_zbb_zbc_zbs --stf_priv_modes USHM --stf_force_zero_sha --stf_disable_memory_records"
# ------------------------------------------------------------------
# bmi_sanity has 7 regions (start/stop pairs) and an extra stop at the end
# ------------------------------------------------------------------

ELFS=(
"zbx.rv64gc_with_zbx.bare.riscv"
"simple.rv64gc_with_zbx.bare.riscv"

"bmi_pmp.rv64gc_nozbx.bare.riscv"
"bmi_pmp.rv64gc_with_zbx.bare.riscv"

"bmi_median.rv64gc_nozbx.bare.riscv"
"bmi_median.rv64gc_with_zbx.bare.riscv"

"bmi_dhrystone.rv64gc_nozbx.bare.riscv"
"bmi_dhrystone.rv64gc_with_zbx.bare.riscv"

"bmi_sanity.rv64gc_nozbx.bare.riscv"
"bmi_sanity.rv64gc_with_zbx.bare.riscv"

"coremark_10.rv64gc_nozbx.bare.riscv"
"coremark_10.rv64gc_with_zbx.bare.riscv"

"dhrystone_opt1.10.rv64gc_nozbx.bare.riscv"
"dhrystone_opt1.10.rv64gc_with_zbx.bare.riscv"

"dhrystone_opt2.10.rv64gc_nozbx.bare.riscv"
"dhrystone_opt2.10.rv64gc_with_zbx.bare.riscv"

"dhrystone_opt3.10.rv64gc_nozbx.bare.riscv"
"dhrystone_opt3.10.rv64gc_with_zbx.bare.riscv"

)

ELF=${ELFS[11]}

# ------------------------------------------------------------------
mkdir -p drom_out
mkdir -p spike_out

rm -f drom_out/*
rm -f spike_out/*
# ------------------------------------------------------------------
$OBJ_DUMP $DOPTS elfs/$ELF > objdump/$ELF.objdump

echo "${ELF}"

echo "SPIKE+STF"
$SPIKE -l --log=exe.log --isa=$ISA --stf_trace spike_out/$ELF.zstf $STF elfs/$ELF
#$STF_DUMP -c spike_out/$ELF.zstf > spike_out/$ELF.stf_dump
#$STF_RDUMP   spike_out/$ELF.zstf > spike_out/$ELF.rstf_dump
#
#echo "DROMAJO"
#$DROM --march=$ISA $DSTF --stf_trace drom_out/$ELF.zstf elfs/$ELF
#$STF_DUMP -c drom_out/$ELF.zstf > drom_out/$ELF.stf_dump
#$STF_RDUMP   drom_out/$ELF.zstf > drom_out/$ELF.rstf_dump
#
#tkdiff spike_out/$ELF.stf_dump drom_out/$ELF.stf_dump &
#tkdiff spike_out/$ELF.rstf_dump drom_out/$ELF.rstf_dump &
