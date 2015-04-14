#ifndef FRAME_H
#define FRAME_H

#include <string.h>
#include "kernel/thread.h"
#include "kernel/loader.h"

int lookuptable[32];
typedef struct {
  void* virtualAddress;
  int t_id;
}fte;
fte* frametable;
void preptable(size_t page_limit);
int f_find_empty_spot(void);
void* aquire_user_page(int id,int zero,int stack);
void free_user_page(void* page);
void set_page_as_free(int index);
void wipe_thread_pages(tid_t id);
#endif
