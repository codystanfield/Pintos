#include "page.h"
#include <debug.h>
#include "kernel/palloc.h"
#include "kernel/vaddr.h"
#include "kernel/malloc.h"
#include "vm/frame.h"

const struct lock load_lock;

void preppagetable(){
  lock_init(&load_lock);
}
Page* file_page(struct file *file, off_t ofs,size_t read_bytes, size_t zero_bytes, bool writable, uint8_t* upage){
  Page *page = (Page*) malloc(sizeof(Page));
  if (page == NULL)
    PANIC("PROBLEM GETTING MEMORY");
  //printf("GETTING FOR %u\n",upage);
  page->loc=FILE;
  page->writeable=writable;
  page->loaded=false;
  page->uaddr=upage;
  page->kpage=NULL;
  page->pagedir=thread_current()->pagedir;
  page->swap_index=-1;
  page->zeroed=false;

  //File specific information
  page->file.file=file;
  page->file.ofs=ofs;
  page->file.read_bytes=read_bytes;
  page->file.zero_bytes=zero_bytes;
  add_page(page);
  return page;
}
Page* zero_page(void* addr, bool writable){
  Page* page = (Page*)malloc(sizeof(Page));
  //printf("GETTING FOR %u\n",addr);

  if(page==NULL)
    return NULL;
  page->loc=NONE;
  page->writeable=writable;
  page->loaded=false;
  page->uaddr=addr;
  page->kpage=NULL;
  page->pagedir=thread_current()->pagedir;
  page->zeroed=true;

  add_page(page);
  return page;
}
bool load_page(Page* page, bool lock){
  //printpagestats(page);
  Frame* f;
  lock_acquire(&load_lock);
  //printf("acquired lock\n");
  //printf("page: %u",page);
  if(page->kpage==NULL){//we have to load it into a frame
    //printf("in the if\n");
    f=get_frame(PAL_USER);
    //printf("GOT THE PAGE!\n");
  }
  //printf("PASTED THE IF\n");
  //printf("released the lock\n");
  page->kpage=f->addr;
  set_page(f->addr,page);
  f->page=page;
  bool worked = true;
  if(page->zeroed==true)
    memset(page->kpage,0,PGSIZE);
  if(page->loc==FILE)
    worked = load_from_file(page->kpage,page);
  else if(page->loc==SWAP){
    //printf("reading from swap!!!!!!! swap index=%u\n",page->swap_index);
    read_page_from_swap(page->swap_index,f->addr);
    page->loc=NONE;
  }
  else if(page->zeroed==true)
    memset(page->kpage,0,PGSIZE);
  lock_release(&load_lock);


  if(!worked)
    unlock_frame(page->kpage);
  //printf("PASED THE IFS\n");
  pagedir_clear_page(page->pagedir, page->uaddr);
  if(!pagedir_set_page(page->pagedir,page->uaddr,page->kpage,page->writeable)){
    ASSERT(false);
    unlock_frame(page->kpage);
    return false;
  }

  page->loaded=true;
  if(!lock)
    unlock_frame(page->kpage);
  return true;

}
void add_page(Page* page){
  pagedir_add_page(page->pagedir, page->uaddr, (void*)page);
}
Page* find_page(void* addr){
  uint32_t* pagedir=thread_current()->pagedir;
  Page* page = NULL;
  page=(Page*)pagedir_find_page(pagedir,(const void*)addr);
  return page;
}
bool load_from_file(uint8_t* kpage,Page* page){
  //printf("made it to load from file\n");
  file_seek(page->file.file,page->file.ofs);
  size_t rsize= file_read(page->file.file,kpage,page->file.read_bytes);
  if(rsize!=page->file.read_bytes){
    free_frame(kpage,page->pagedir);
    return false;
  }
  memset(kpage+page->file.read_bytes,0,page->file.zero_bytes);
  return true;
}
void printpagestats(Page* page){
  printf("Page loc: %d\n",page->loc);
  printf("Page writeable: %d\n",page->writeable);
  printf("Page loaded: %d\n",page->loaded);
  printf("Page uaddr: %u\n",page->uaddr);
  printf("Page kpage: %u\n",page->kpage);
  printf("Page pagedir: %u Current Pagedir: %u\n",thread_current()->pagedir,page->pagedir);
  printf("Page zeroed: %u\n",page->zeroed);
}
