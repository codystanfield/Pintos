#include <debug.h>
#include <stdio.h>
#include "kernel/malloc.h"
#include "kernel/palloc.h"
#include "kernel/pagedir.h"
#include "kernel/vaddr.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"


static struct lock page_lock;
extern uint32_t userpool_base_addr;
/* Initializes the lock for our page functions*/
void preppagetable(){
  lock_init(&page_lock);
}

/* Create a page representing a segment of a file. It contains all the nessacary information
   to later load that segment into memory. The need params were taken from the load segment
   where it read the file into memory.*/
Page* file_page(struct file *file, off_t ofs,size_t read_bytes, size_t zero_bytes, bool writable, uint8_t* upage){
  Page *page = (Page*) malloc(sizeof(Page));
  if (page == NULL)
    return NULL;
  page->loc=FILE;
  page->writeable=writable;
  page->loaded=false;
  page->uaddr=upage;
  page->pagedir=thread_current()->pagedir;
  page->swap_index=-1;
  page->frame_index=-1;
  //File specific information
  page->file.file=file;
  page->file.ofs=ofs;
  page->file.read_bytes=read_bytes;
  page->file.zero_bytes=zero_bytes;
  pagedir_custom_page(page->pagedir, page->uaddr, (void*)page);
  return page;
}

/* Creates a zeroed out page entry. Useful for when we want to expand the stack*/

Page* zero_page(void* addr, bool writable){
  Page* page = (Page*)malloc(sizeof(Page));
  if(page==NULL)
    return NULL;
  page->loc=ZERO;
  page->writeable=writable;
  page->loaded=false;
  page->uaddr=addr;
  page->frame_index=-1;
  page->pagedir=thread_current()->pagedir;
  pagedir_custom_page(page->pagedir, page->uaddr, (void*)page);
  return page;
}

/* Loads a page into memory then either unlocks or keeps locked the frame
   it is using based on the request of the caller. This is basically to
   allow a load in a system call to not have to worry about its frame while
   it is being written or retrieved from a file.*/
bool load_page(Page* page, bool lock){
  int i=-1;
  lock_acquire(&page_lock);
  if(page->frame_index==-1)
    i=get_frame(PAL_USER);
  page->frame_index=i;
  frametable[i].page=page;
  /* This if statement loads each type of file based on where it is
     in the system. Whether it been in swap a file or if its zero'd out*/
  if(page->loc==FILE)
    load_from_file((void*)(i*PGSIZE+userpool_base_addr),page);
  else if(page->loc==SWAP){
    read_page_from_swap(page->swap_index,(void*)(i*PGSIZE+userpool_base_addr));
    page->loc=NONE;
    page->swap_index=0;
  }
  else if(page->loc==ZERO)
    memset((void*)(i*PGSIZE+userpool_base_addr),0,PGSIZE);
  /*clear out the old entry and but in the new entry into pagedir*/
  pagedir_clear_page(page->pagedir, page->uaddr);
  pagedir_set_page(page->pagedir,page->uaddr,(void*)(i*PGSIZE+userpool_base_addr),page->writeable);
  page->loaded=true;
  /*We're done working on the page so we release the lock*/
  lock_release(&page_lock);


  /* If the caller doesnt want the page to remain locked we unlock it*/
  if(lock==false)
    unlock_frame(i);
  return true;
}
/* A little bit of abstraction to make the code easier to read. Loads the
   page from a file into kpage.*/
void load_from_file(void* kpage,Page* page){
  lock_acquire (&thread_filesys_lock);
  file_seek(page->file.file,page->file.ofs);
  file_read(page->file.file,kpage,page->file.read_bytes);
  lock_release (&thread_filesys_lock);
  memset(kpage+page->file.read_bytes,0,page->file.zero_bytes);
}
/* Debuging function. Prints out various stats for a page*/
void printpagestats(Page* page){
  printf("Page loc: %d\n",page->loc);
  printf("Page writeable: %d\n",page->writeable);
  printf("Page loaded: %d\n",page->loaded);
  printf("Page uaddr: %p\n",page->uaddr);
  printf("Page pagedir: %p Current Pagedir: %p\n",thread_current()->pagedir,page->pagedir);
}
