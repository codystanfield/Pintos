#include "page.h"
#include <debug.h>
#include "kernel/palloc.h"
#include "kernel/vaddr.h"

/* preppagetable gets run at startup and preps the page table sup_page_table, meaning it
   sets each item in each page table entry to -1, signifying they're empty. */

void preppagetable(){
  int i;
  for(i=0;i<512;i++){
    sup_page_table[i].attr=-1;
    sup_page_table[i].location=-1;
    sup_page_table[i].id=-1;
  }
}

/* p_find_empty_slot parses the entire page array and looks for the first empty slot,
   then returns its location in the array. If there aren't any more empty slots, it panics the kernel. */

int p_find_empty_slot(){
    int i;
    for(i=0;i<512;i++){
      if(sup_page_table[i].attr==-1)
        return i;
    }
    PANIC("NO EMPTY SUPPLEMENTAL PAGE TABLE SLOT"); // no more empty slots
}

/* add_entry takes an int 'location', a thread id 'id', and a char 'attr', and adds the given values
   to the next empty slot in the page table, found by calling p_find_empty_slot. */

void add_entry(int location,tid_t id,char attr){
  int i=p_find_empty_slot();
  sup_page_table[i].attr=attr;
  sup_page_table[i].location=location;
  sup_page_table[i].id=id;
}

/* free_thread_id takes a thread id 'id' and frees the given thread from the page table, allowing
   use with another process. */

void free_thread_id(tid_t id){
  int i;
  for(i=0;i<512;i++){
    if(sup_page_table[i].id==id){
      sup_page_table[i].id=-1;
      sup_page_table[i].location=-1;
      sup_page_table[i].attr=-1;
    }
  }
}
