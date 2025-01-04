    .section .data
    .align 4

# Define the input vectors (initialized with some values)
vector1:    .word 1, 2, 3, 4, 5, 6, 7, 8
vector2:    .word 8, 7, 6, 5, 4, 3, 2, 1
result:     .space 32                # Reserve space for 8 results (8 x 4 bytes)

    .section .text
    .globl _start

_start:
    # Load base addresses of the vectors into registers
    la t0, vector1              # t0 = address of vector1
    la t1, vector2              # t1 = address of vector2
    la t2, result               # t2 = address of result

    # Initialize the vector unit
    li t3, 8                    # t3 = number of elements (8)
    vsetvli t0, t3, e32         # Set vector length to 8, element width to 32 bits

    # Load the input vectors into vector registers
    vlw.v v1, (t0)              # Load vector1 into v1
    vlw.v v2, (t1)              # Load vector2 into v2

    # Perform vector addition
    vadd.vv v3, v1, v2          # v3 = v1 + v2

    # Store the result back to memory
    vsw.v v3, (t2)              # Store vector v3 to result

    # End of program (trap to exit)
    li a7, 93                   # a7 = ecall for exit
    li a0, 0                    # a0 = exit code 0
    ecall

