#include "page.h"
#include <debug.h>
#include "kernel/palloc.h"
#include "kernel/vaddr.h"
#include "kernel/malloc.h"
#include "vm/frame.h"
#include "kernel/interrupt.h"

static struct lock load_lock;

void preppagetable(){
  lock_init(&load_lock);
}
Page* file_page(struct file *file, off_t ofs,size_t read_bytes, size_t zero_bytes, bool writable, uint8_t* upage){

  Page *page = (Page*) malloc(sizeof(Page));
  if (page == NULL)
    PANIC("PROBLEM GETTING MEMORY");
  page->loc=FILE;
  page->writeable=writable;
  page->loaded=false;
  page->uaddr=upage;
  page->kpage=NULL;
  page->pagedir=thread_current()->pagedir;
  page->swap_index=-1;
  page->zeroed=false;
  page->frame_index=-1;

  //File specific information
  page->file.file=file;
  page->file.ofs=ofs;
  page->file.read_bytes=read_bytes;
  page->file.zero_bytes=zero_bytes;
  add_page(page);
  return page;
}
Page* zero_page(void* addr, bool writable){
//  lock_acquire(&load_lock);

  Page* page = (Page*)malloc(sizeof(Page));
  //printf("GETTING FOR %u\n",addr);

  if(page==NULL)
    return NULL;
  page->loc=ZERO;
  page->writeable=writable;
  page->loaded=false;
  page->uaddr=addr;
  page->kpage=NULL;
  page->frame_index=-1;
  page->pagedir=thread_current()->pagedir;
  //page->zeroed=true;

  add_page(page);
//  lock_release(&load_lock);

  return page;
}
bool load_page(Page* page, bool lock){

  lock_acquire(&load_lock);
  int i=-1;
  if(page->kpage==NULL){//we have to load it into a frame
    //printf("in the if\n");
    i=get_frame(PAL_USER);//f=get_frame(PAL_USER);
    //printf("GOT THE PAGE!\n");
  }
  ASSERT(frametable[i].framelock==true);
  ASSERT(i!=-1);
  //printf("PASTED THE IF\n");
  //printf("released the lock\n");
  page->kpage=frametable[i].addr;
  page->frame_index=i;

  set_page(i,page);
  //ASSERT(frametable[i].framelock==true);
  //ASSERT(frametable[i].page==page);
  //f->page=page;
  bool worked = true;
  if(page->loc==FILE){
    //printf("LOADING FROM FILE\n");
    worked = load_from_file(page->kpage,page);
    //page->loc=NONE;
  }
  else if(page->loc==SWAP){
    //printf("reading from swap!!!!!!! swap index=%u\n",page->swap_index);
    read_page_from_swap(page->swap_index,frametable[i].addr);
    //page->loc=NONE;
  }
  else if(page->loc==ZERO){
    memset(page->kpage,0,PGSIZE);
    //page->loc=NONE;
  }

  else
    PANIC("IDK");
  //lock_release(&load_lock);


  /*if(!worked)
    unlock_frame(page->kpage);*/
  //printf("PASED THE IFS\n");
  pagedir_clear_page(page->pagedir, page->uaddr);
  if(!pagedir_set_page(page->pagedir,page->uaddr,page->kpage,page->writeable)){
    ASSERT(false);
    unlock_frame(i);
    lock_release(&load_lock);
    return false;
  }

  page->loaded=true;
  if(lock==false)
    unlock_frame(i);
  lock_release(&load_lock);
  return true;

}
void add_page(Page* page){
  pagedir_add_page(page->pagedir, page->uaddr, (void*)page);
}
Page* find_page(void* addr){
  lock_acquire(&load_lock);
  uint32_t* pagedir=thread_current()->pagedir;
  Page* page = NULL;
  page=(Page*)pagedir_find_page(pagedir,(const void*)addr);
  lock_release(&load_lock);
  //printf("%u\n",page->uaddr);
  return page;
}
bool load_from_file(void* kpage,Page* page){
  //printf("made it to load from file\n");
  lock_acquire (&thread_filesys_lock);
  file_seek(page->file.file,page->file.ofs);
  size_t rsize= file_read(page->file.file,kpage,page->file.read_bytes);
  if(rsize!=page->file.read_bytes){
    //free_frame(kpage,page->pagedir);
    lock_release (&thread_filesys_lock);
    return false;
  }
  memset(kpage+page->file.read_bytes,0,page->file.zero_bytes);
  lock_release (&thread_filesys_lock);
  return true;
}
void printpagestats(Page* page){
  printf("Page loc: %d\n",page->loc);
  printf("Page writeable: %d\n",page->writeable);
  printf("Page loaded: %d\n",page->loaded);
  printf("Page uaddr: %p\n",page->uaddr);
  printf("Page kpage: %p\n",page->kpage);
  printf("Page pagedir: %p Current Pagedir: %p\n",thread_current()->pagedir,page->pagedir);
  printf("Page zeroed: %u\n",page->zeroed);
}
