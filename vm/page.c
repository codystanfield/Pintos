#include "page.h"
#include <debug.h>
#include "kernel/palloc.h"
#include "kernel/vaddr.h"

// preps that page table, setting all values to -1 (default)
void preppagetable(){
  int i;
  for(i=0;i<512;i++){
    sup_page_table[i].id=-1
    sup_page_table[i].attr=-1;
    sup_page_table[i].location=-1;
  }
}

// finds the first empty slot in the supp page table
int find_empty_slot(){
    int i;
    for(i=0;i<512;i++){
      if(sup_page_table[i].attr==-1)
        return i;
    }
    PANIC("NO EMPTY SUPPLEMENTAL PAGE TABLE SLOT");
}

// adds an entry into the supplemental page table
void add_entry(int location,tid_t id,char attr){
  int i=find_empty_slot();
  sup_page_table[i].attr=attr;
  sup_page_table[i].location=location;
  sup_page_table[i].id=id;
}

// frees a slot in the page table when given the page's id
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
