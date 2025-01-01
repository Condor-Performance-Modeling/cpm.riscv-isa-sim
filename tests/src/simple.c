
int main() {
    int result = 0;

    asm volatile (
        "xor x0,x0,x0              \n" // start trace
        "csrr    x5,mstatus        \n" // x5 = mstatus
        "addi    x0, x0, 0         \n" // nop
        "addi    x0, x0, 0         \n" // nop
        "addi    x0, x0, 0         \n" // nop
        "addi    x0, x0, 0         \n" // nop
        "addi    x0, x0, 0         \n" // nop
        "addi    x0, x0, 0         \n" // nop
        "addi    x0, x0, 0         \n" // nop
        "addi    x0, x0, 0         \n" // nop
        "addi    x0, x0, 0         \n" // nop
        "addi    x0, x0, 0         \n" // nop

        "csrrwi  x0,pmpcfg0,0      \n" // pmpcfg0 = 0
        "c.li    x15,-1            \n" // x15 = 0xFFFFFFFFFFFFFFFF
        "csrrw   x0,pmpaddr0,x15   \n" // pmpaddr0 = -1
        "csrrs   x12,pmpaddr0,x0   \n" // x12 = pmpaddr0

        "c.addiw x11,-1            \n" //

        "beq x11, x0, skip         \n" // X11 != 0

        "addi x6, x6, 1            \n" //  not executed
        "addi x6, x6, 1            \n" // 
        "addi x6, x6, 1            \n" // 
        "addi x6, x6, 1            \n" // 

     "skip: \n"
        "addi x6, x6, 1            \n" // executed
        "addi x6, x6, 1            \n" // 
        "addi x6, x6, 1            \n" // 
        "addi x6, x6, 1            \n" // 

        "j skip2                   \n"

        "addi x6, x6, 1            \n" // not executed
        "addi x6, x6, 1            \n" // 
        "addi x6, x6, 1            \n" // 
        "addi x6, x6, 1            \n" // 

     "skip2: \n"
        "li x5, 0                  \n" // Initialize x5 (x5) to 0
        "addi x5, x5, 1            \n" // Increment by 1
        "addi x5, x5, 1            \n" // 
        "addi x5, x5, 1            \n" // 
        "addi x5, x5, 1            \n" // 
        "addi x5, x5, 1            \n" // 
        "addi x5, x5, 1            \n" // 
        "addi x5, x5, 1            \n" // 
        "addi x5, x5, 1            \n" // 
        "addi x5, x5, 1            \n" // 
        "addi x5, x5, 1            \n" // 
        "addi x5, x5, 1            \n" // 
        "addi x5, x5, 1            \n" // 
        "addi x5, x5, 1            \n" // 
        "addi x5, x5, 1            \n" // 
        "addi x5, x5, 1            \n" // 
        "addi x5, x5, 1            \n" // 

        "mv %0, x5                 \n" // Move result to output variable
        "xor x0,x1,x1              \n" // stop trace
        : "=r" (result)                // Output: result
        :                              // No inputs
        : "t0"                         // Clobbered register
    );

    return 0;
}

