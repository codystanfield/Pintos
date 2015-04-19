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
//#include "kernel/pagedir.c"

static struct lock f_lock;
static struct lock e_lock;
struct hash frames;
bool frame_less (const struct hash_elem *, const struct hash_elem *, void *);
unsigned frame_hash (const struct hash_elem *, void *);

void preptable(){
  lock_init(&f_lock);
  lock_init(&e_lock);
  hash_init(&frames, frame_hash, frame_less, NULL);
}
void* get_frame(enum palloc_flags flags){
  //printf("in get frame\n");
  void *frame = palloc_get_page(flags);
  if(frame!=NULL){
    Frame* f = (Frame*)malloc(sizeof(Frame));
    if(f==NULL)
      return false;

    f->addr=frame;
    lock_acquire(&f_lock);
    hash_insert(&frames,&f->hash_elem);
    lock_release(&f_lock);
  }
  else{
    PANIC("NO FREE FRAME");
  }
  return frame;
}
Page* get_page(void* frame, uint32_t* pagedir){
  Frame *f = find_entry(frame);
  Page* page = f->u_virtualAddress;
  return NULL;

}
void unlock_frame(void* addr){
  Frame* f = find_entry(addr);
  if(f!=NULL){
    f->framelock=false;
  }
}
void lock_frame(void* addr){
  Frame* f= find_entry(addr);
  if(f!=NULL){
    f->framelock=true;
  }
}
bool set_page(void* frame, Page* page){
  Frame* f = find_entry(frame);
  if(f==NULL)
    return false;
  f->u_virtualAddress=page->uaddr;
  return true;
}
Frame* find_entry(void* addr){
  Frame f;
  struct hash_elem *elem;
  f.addr=addr;
  lock_acquire(&f_lock);
  elem=hash_find(&frames,&f.hash_elem);
  lock_release(&f_lock);
  if(elem!=NULL)
    return hash_entry(elem,Frame,hash_elem);
  else
    return NULL;
}
void free_frame(void* kpage, uint32_t pagedir){
  lock_acquire(&e_lock);
  Frame* f= find_entry(kpage);
  if(f==NULL){
    lock_release(&e_lock);
  }
  lock_acquire (&f_lock);
  hash_delete (&frames, &f->hash_elem);
	free (f);
  lock_release (&f_lock);
  lock_release(&e_lock);
}

unsigned frame_hash(const struct hash_elem *f, void* a){
  const Frame* frame= hash_entry(f,Frame,hash_elem);
  return hash_int((unsigned)frame->addr);
}
bool frame_less(const struct hash_elem *a_, const struct hash_elem *b_,void *aux){
  const Frame* a = hash_entry(a_,Frame,hash_elem);
  const Frame* b = hash_entry(b_,Frame,hash_elem);
  return a->addr< b->addr;
}
