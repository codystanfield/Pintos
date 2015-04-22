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
  Page* page;
  bool framelock;

}Frame;

Frame frametable[383];

// preps the functions
void preptable(void);
int get_frame(enum palloc_flags flags);
Page* recover_page(void* kpage);
bool set_page(int i, Page* page);
void unlock_frame(int i);
void evict_r_frame(void);
void lock_frame(int i);
#endif
