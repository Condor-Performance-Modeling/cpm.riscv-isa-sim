
#define START_TRACE "xor x0,x0,x0"
#define STOP_TRACE  "xor x0,x1,x1"

int main() {
    int result = 0;

    asm volatile (
    "prolog: \n"
      "li x5, 0                  \n"
      "addi x5, x5, 1            \n" // Increment by 1
      "addi x5, x5, 2            \n" // 
      "addi x5, x5, 3            \n" // 
      "addi x5, x5, 4            \n" // 
    "region1: \n"
      "xor x0,x0,x0              \n" // start trace
      "addi x5, x5, 5            \n" // Increment by 1
      "addi x5, x5, 6            \n" // 
      "addi x5, x5, 7            \n" // 
      "addi x5, x5, 8            \n" // 
      "xor x0,x1,x1              \n" // stop trace
    "region2: \n"
      "xor x0,x0,x0              \n" // start trace
      "addi x5, x5, 9            \n" // Increment by 1
      "addi x5, x5, 10           \n" // 
      "addi x5, x5, 11           \n" // 
      "addi x5, x5, 12           \n" // 
      "xor x0,x1,x1              \n" // stop trace
    "region3: \n"
      "xor x0,x0,x0              \n" // start trace
      "addi x5, x5, 13           \n" // Increment by 1
      "addi x5, x5, 14           \n" // 
      "addi x5, x5, 15           \n" // 
      "addi x5, x5, 16           \n" // 
      "xor x0,x1,x1              \n" // stop trace
    "region4: \n"
      "xor x0,x0,x0              \n" // start trace
      "addi x5, x5, 17           \n" // Increment by 1
      "addi x5, x5, 18           \n" // 
      "addi x5, x5, 19           \n" // 
      "addi x5, x5, 20           \n" // 
      "xor x0,x1,x1              \n" // stop trace
    "region5: \n"
      "xor x0,x0,x0              \n" // start trace
      "addi x5, x5, 21           \n" // Increment by 1
      "addi x5, x5, 22           \n" // 
      "addi x5, x5, 23           \n" // 
      "addi x5, x5, 24           \n" // 
      "xor x0,x1,x1              \n" // stop trace

    "epilog: \n"
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

