#ifndef FRAME_H
#define FRAME_H

#include <string.h>
#include "kernel/thread.h"
#include "kernel/loader.h"

int lookuptable[16];
typedef struct {
  void* virtualAddress;
  int id;
}fte;
fte* frametable;
int page_limit;
void preptable(size_t page_limit);
int f_find_empty_spot(tid_t id);
void* aquire_user_page(tid_t id,int zero,int stack);
void free_user_page(void* page);
void set_page_as_free(int index);
void wipe_thread_pages(tid_t id);
int page_fault_handler(tid_t id);
#endif
