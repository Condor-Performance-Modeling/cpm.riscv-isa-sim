#include <stdint.h>
#include <sys/signal.h>

extern volatile uint64_t tohost;
extern volatile uint64_t fromhost;
void __attribute__((noreturn)) tohost_exit(uintptr_t code)
{
  tohost = (code << 1) | 1;
  while (1);
}
uintptr_t __attribute__((weak)) handle_trap(uintptr_t cause, 
                                uintptr_t epc, uintptr_t regs[32])
{
  tohost_exit(1337);
}
void exit(int code) { tohost_exit(code); }
void abort()        { exit(128 + SIGABRT); }
int __attribute__((weak)) main(int argc, char** argv) { return -1; }
void _init(int cid, int nc)
{
  int ret = main(0, 0);
  exit(ret);
}
