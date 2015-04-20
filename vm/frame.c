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
static struct hash frametable;
static bool frame_less (const struct hash_elem *, const struct hash_elem *, void *);
static unsigned frame_hash (const struct hash_elem *, void *);

void preptable(){
  lock_init(&f_lock);
  lock_init(&e_lock);
  hash_init(&frametable, frame_hash, frame_less, NULL);
  random_init(9867);
}
Frame* get_frame(enum palloc_flags flags){
  //printf("in get frame\n");
  void *addr = palloc_get_page(flags);
  if(addr!=NULL){
    Frame* f = (Frame*)malloc(sizeof(Frame));
    if(f==NULL)
      return false;

    f->addr=addr;
    //printf("getting kernel addr:%u\n",addr);
    f->framelock=true;
    lock_acquire(&f_lock);
    hash_insert(&frametable,&f->hash_elem);
    lock_release(&f_lock);
    return f;
  }
  else{
    //printf("NO FREE FRAME");
    //PANIC("NO FREE FRAME");
    return evict_r_frame();
  }

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
  elem=hash_find(&frametable,&f.hash_elem);
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
  hash_delete (&frametable, &f->hash_elem);
	free (f);
  lock_release (&f_lock);
  lock_release(&e_lock);
}
Frame* evict_r_frame(){
  Frame* f=NULL;
  int i=random_ulong()%383;
  //printf("evicting a frame i=%d\n",i);
  int z;
  lock_acquire(&e_lock);

  struct hash_iterator iter;
  hash_first(&iter,&frametable);
  hash_next(&iter);

  for(;i>0;i--){
      hash_next(&iter);
      //f =hash_entry(hash_cur (&iter), Frame,hash_elem);
      //printf("Z=%d at add: %u and page addr:%u\n",z,f->addr,f->page->uaddr);
    }

  f = hash_entry(hash_cur(&iter),Frame,hash_elem);
  if(f->framelock==true){
    lock_release(&e_lock);
    return evict_r_frame();
  }
  //printf("frame kpage is: %u\n",f->addr);
  Page* p = f->page;
  p->loc=SWAP;
  p->swap_index=s_find_empty_slot();
  f->framelock=true;
  //printf("WRITING TO SWAP AT INDEX: %u\n",p->swap_index);
  write_page_to_swap(f->addr,p->swap_index);
  lock_release(&e_lock);

  pagedir_clear_page (p->pagedir, p->uaddr);
  pagedir_add_page (p->pagedir, p->uaddr, (const void *)p);
  p->loaded = false;
  p->kpage = NULL;

  return f;

}

static unsigned frame_hash (const struct hash_elem *f_, void *aux UNUSED)
{
  const Frame *f = hash_entry (f_, Frame, hash_elem);
  return hash_int ((unsigned)f->addr);
}
static bool frame_less (const struct hash_elem *a_, const struct hash_elem *b_,
               void *aux UNUSED)
{
  const Frame *a = hash_entry (a_, Frame, hash_elem);
  const Frame *b = hash_entry (b_, Frame, hash_elem);

  return a->addr < b->addr;
}
