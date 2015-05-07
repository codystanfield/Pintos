#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include "kernel/thread.h"

tid_t process_execute (const char *cmdline);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* kernel/process.h */
