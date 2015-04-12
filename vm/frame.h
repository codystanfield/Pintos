#ifndef FRAME_H
#define FRAME_H

#include <string.h>
#include "kernel/thread.h"
long long lookuptable[32];
struct fte{
  void* virtualAddress;
  int t_id;
};
struct fte frametable[1024];
void preptable(void);
int find_empty_spot(void);
void* aquire_user_page(int id,int zero);
void free_user_page(void* page);
void set_page_as_free(int index);
void wipe_thread_pages(tid_t id);
#endif
