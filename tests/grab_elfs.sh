SRC=../../benchmarks/bin
DST=./elfs

#X=rv64gc_nozbx
X=rv64gc_with_zbx

ELFS=(
"bmi_sanity"
"bmi_dhrystone"
"bmi_median"
"bmi_pmp"
"coremark_10"
"dhrystone_opt1.10"
"dhrystone_opt2.10"
"dhrystone_opt3.10"
)


for ELF in "${ELFS[@]}"; do
  cp "${SRC}/${ELF}.bare.riscv" "${DST}/${ELF}.${X}.bare.riscv"
done
