#include "vm/frame.h"
#include <debug.h>
#include "kernel/palloc.h"
#include "kernel/vaddr.h"
#include "vm/page.h"
#include "kernel/malloc.h"
#include "lib/random.h"
#include "kernel/pagedir.h"
#include "kernel/synch.h"
#include "vm/swap.h"

static struct lock frame_lock;

extern uint32_t userpool_base_addr;
/* Preps our frametable.*/
void preptable(){
  lock_init(&frame_lock);
  int i;
  for(i=0;i<383;i++){
    frametable[i].addr=NULL;
    frametable[i].page=NULL;
    frametable[i].framelock=false;
  }
  random_init(0);
}

/* Attempts to obtain a frame. If it cannot it will evict one
   and try again.*/
int get_frame(enum palloc_flags flags){
  void* addr = palloc_get_page(flags);
  if(addr!=NULL){
    int i=((uint32_t)addr-userpool_base_addr)/PGSIZE;
    lock_acquire(&frame_lock);
    frametable[i].addr=addr;
    frametable[i].page=NULL;
    frametable[i].framelock=true;
    lock_release(&frame_lock);
    return i;
  }
  else{
    evict_r_frame();
    return get_frame(flags);
  }
}
/* Frees a frame. This function should only be called
   when destroying a process's pagedir*/
void free_frame(void* addr){
  int i =((uint32_t)addr-userpool_base_addr)/PGSIZE;
  lock_acquire(&frame_lock);
  frametable[i].addr=NULL;
  frametable[i].page=NULL;
  frametable[i].framelock=false;
  palloc_free_page(addr);
  lock_release(&frame_lock);
}
/* Unlocks a frame*/
void unlock_frame(int i){
  ASSERT(i>=0);
  frametable[i].framelock=false;
}
/* Locks a frame*/
void lock_frame(int i){
  ASSERT(i>=0);
  frametable[i].framelock=true;
}
/* Evicts a random frame. If the frame's page is dirty it
   will be written to swap otherwise it will be set to where
   it orginally came from be it a zero page or a file.*/
void evict_r_frame(){
  int i=random_ulong()%383;
  lock_acquire(&frame_lock);
  while(1){
    if(frametable[i].framelock==false){
      frametable[i].framelock=true;
      break;
    }
    else
     i=random_ulong()%383;
  }
  lock_release(&frame_lock);
  Page* p = frametable[i].page;
  if(pagedir_is_dirty(p->pagedir,frametable[i].addr)){
    p->loc=SWAP;
    pagedir_clear_page(p->pagedir, p->uaddr);
    pagedir_custom_page(p->pagedir, p->uaddr,(void *)p);
    p->loaded = false;
    p->swap_index=write_page_to_swap((void*)frametable[i].addr);
  }
  else if(p->loc==FILE||p->writeable==false){
    pagedir_clear_page(p->pagedir, p->uaddr);
    pagedir_custom_page(p->pagedir, p->uaddr,(void *)p);
    p->loaded = false;
  }

  p->frame_index=-1;
  lock_acquire(&frame_lock);
  palloc_free_page(frametable[i].addr);
  frametable[i].addr=NULL;
  frametable[i].page=NULL;
  lock_release(&frame_lock);
}
