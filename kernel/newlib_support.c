#include <errno.h>
#include <sys/types.h>

void _exit(void) {
  while (1) __asm__("hlt");
}

// malloc/free of Newlib depend on program_break and program_break_end
caddr_t program_break, program_break_end;

// update program break
caddr_t sbrk(int incr) {
  // if program break is 0, the variable is not initialized correctly
  // if program break + incr >= program_break_end, there is no enough memory left
  if (program_break == 0 || program_break + incr >= program_break_end) {
    errno = ENOMEM;
    return (caddr_t)-1;
  }

  // if there is no problem, return previous program break value and update program break value
  caddr_t prev_break = program_break;
  program_break += incr;
  return prev_break;
}

int getpid(void) {
  return 1;
}

int kill(int pid, int sig) {
  errno = EINVAL;
  return -1;
}
