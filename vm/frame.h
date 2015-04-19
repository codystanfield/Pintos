#ifndef FRAME_H
#define FRAME_H

#include <string.h>
#include "kernel/thread.h"
#include "kernel/loader.h"
#include "kernel/palloc.h"
#include <hash.h>
#include "vm/page.h"


typedef struct {
  void* addr;
  struct hash_elem hash_elem;
  void* u_virtualAddress;
  bool framelock;
  struct lock list_lock;
  struct list_elem list_elem;
}Frame;



// preps the functions
void preptable(void);
void* get_frame(enum palloc_flags flags);
void unlock_frame(void* addr);
void lock_frame(void* addr);
Frame* find_entry(void* addr);
void free_frame(void* kpage, uint32_t pagedir);


#endif
