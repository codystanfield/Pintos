#ifndef KERNEL_TSS_H
#define KERNEL_TSS_H

#include <stdint.h>

struct tss;
void tss_init (void);
struct tss *tss_get (void);
void tss_update (void);

#endif /* kernel/tss.h */
