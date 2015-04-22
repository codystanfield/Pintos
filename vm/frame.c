#include "vm/frame.h"
#include <debug.h>
#include "kernel/palloc.h"
#include "kernel/vaddr.h"
#include <stdio.h>
#include "page.h"
#include "kernel/loader.h"
#include "kernel/exception.h"
#include "kernel/malloc.h"
#include "lib/random.h"
#include "kernel/pagedir.h"
#include "kernel/synch.h"

static struct lock f_lock;
static struct lock e_lock;

extern userpool_base_addr;
void preptable(){
  lock_init(&f_lock);
  lock_init(&e_lock);

  lock_acquire(&f_lock);
  int i;
  for(i=0;i<383;i++){
    frametable[i].addr=NULL;
    frametable[i].page=NULL;
    frametable[i].framelock=false;
  }
  random_init(0);//9867
  lock_release(&f_lock);
}
int get_frame(enum palloc_flags flags){
  //printf("in get frame\n");


  void* addr = palloc_get_page(flags);
  if(addr!=NULL){
    int i=((uint32_t)addr-userpool_base_addr)/PGSIZE;
    lock_acquire(&f_lock);
    frametable[i].addr=addr;
    frametable[i].page=NULL;
    frametable[i].framelock=true;
    lock_release(&f_lock);
    return i;
  }
  else{
    evict_r_frame();

    //lock_release(&f_lock);
    return get_frame(flags);
  }

}
Page* recover_page(void* kpage){
  //printf("most recover page\n");
  lock_acquire(&f_lock);
  Page* page = frametable[((uint32_t)kpage-userpool_base_addr)/PGSIZE].page;
  lock_release(&f_lock);
  return page;
}

void unlock_frame(int i){
  lock_acquire(&f_lock);
  //ASSERT(frametable[((uint32_t)addr-userpool_base_addr)/PGSIZE].framelock==true);
  ASSERT(i>=0);
  frametable[i].framelock=false;
  lock_release(&f_lock);
}
void lock_frame(int i){
  lock_acquire(&f_lock);
  //ASSERT(frametable[((uint32_t)addr-userpool_base_addr)/PGSIZE].framelock==false);
  ASSERT(i>=0);
  frametable[i].framelock=true;
  lock_release(&f_lock);

}
bool set_page(int i, Page* page){//bool set_page(void* frame,Page* page)
  lock_acquire(&f_lock);
  ASSERT(frametable[i].framelock==true);
  frametable[i].page=page;
  lock_release(&f_lock);
  return true;
}
void evict_r_frame(){
  lock_acquire(&e_lock);


  int i=random_ulong()%382;
  lock_acquire(&f_lock);
  while(1){
    if(frametable[i].framelock==false){
      frametable[i].framelock=true;
      break;
    }
    else
     i=random_ulong()%382;

  }
  //ASSERT(i!=383)

  //lock_acquire(&f_lock);
  //ASSERT(frametable[i].framelock==false);
  //frametable[i].framelock=true;
  //ASSERT(frametable[i].addr!=NULL);
  Page* p = frametable[i].page;
  p->loc=SWAP;
  //p->kpage=NULL;''
  pagedir_clear_page(p->pagedir, p->uaddr);
  pagedir_add_page(p->pagedir, p->uaddr,(void *)p);
  p->loaded = false;
  p->kpage = NULL;
  p->swap_index=write_page_to_swap((void*)frametable[i].addr);
  p->frame_index=-1;
  memset(frametable[i].addr,0,PGSIZE);
  //lock
  //lock_acquire(&f_lock);
  palloc_free_page(frametable[i].addr);
  frametable[i].addr=NULL;
  frametable[i].page=NULL;
  lock_release(&f_lock);
  lock_release(&e_lock);
}
void clear_frames_for_pd(void* pd){
  lock_acquire(&f_lock);
  printf("IN CLEAR FRAMES\n");
  int i;
  for(i=0;i<383;i++){
    Page* p = frametable[i].page;
    if(p!=NULL){
      if(p->pagedir==pd){
        free(p);
        //palloc_free_page(frametable[i].addr);
        frametable[i].addr=NULL;
        frametable[i].page=NULL;
        frametable[i].framelock=false;
      }
    }
  }
  lock_release(&f_lock);
}
