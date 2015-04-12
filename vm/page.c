#include "page.h"
#include <debug.h>
#include "kernel/palloc.h"
#include "kernel/vaddr.h"

void preppagetable(){
  int i;
  for(i=0;i<512;i++){
    sup_page_table[i].attr=-1;
    sup_page_table[i].location=-1;
  }
}
int find_empty_slot(){
    int i;
    for(i=0;i<512;i++){
      if(sup_page_table[i].attr==-1)
        return i;
    }
    PANIC("NO EMPTY SUPPLEMENTAL PAGE TABLE SLOT");
}

void add_entry(int location,tid_t id,char attr){
  int i=find_empty_slot();
  sup_page_table[i].attr=attr;
  sup_page_table[i].location=location;
  sup_page_table[i].id=id;
}

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
