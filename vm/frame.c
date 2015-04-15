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


int frameserved;

void preptable(size_t page_limit_t){
  frameserved=0;
  int i;
  for(i=0;i<16;i++){
    lookuptable[i]=~0;
    //printf("%d\n",lookuptable[i]);
  }
  page_limit=383;
  frametable=malloc(sizeof(fte)*10);
  for(i=0;i<5;i++){
    frametable[i].id=-1;
    frametable[i].virtualAddress=-1;
  }
}
int f_find_empty_spot(tid_t id){
  int i;
  int t;
  /*for(i=0;i<16;i++){

    t= __builtin_ffsl(lookuptable[i]);
    if(t!=0){
      lookuptable[i]=lookuptable[i]&(~(1<<(t-1)));
      return i*32+t-1;
    }
    //printf("%d\n",t);
  }
  */
  for(i=0;i<10;i++){
    if(frametable[i].id==-1)
      return i;
  }
  //printf("%d\n",t);
  //page_fault();
  //return page_fault_handler(id);
  //PANIC("NO FREE FRAME");
  return page_fault_handler(id);
}
void* aquire_user_page(tid_t id,int zero,int stack){
  int index = f_find_empty_spot(id);
  frameserved+=1;
  if(zero==1)
    frametable[index].virtualAddress=palloc_get_page(PAL_USER);
  else
    frametable[index].virtualAddress=palloc_get_page(PAL_USER|PAL_ZERO);

    frametable[index].id=id;
  //printf("SERVeING FRAME NUMBER %d\n",frameserved);

  add_entry(index,id,4|stack);
  //in process need to replace line 526 and 486 at least
  return frametable[index].virtualAddress;
}

void free_user_page(void* page){
  int i;
  for(i=0;i<page_limit;i++){
    if(frametable[i].virtualAddress==page){
      set_page_as_free(i);
      palloc_free_page(page);
      return;
    }
  }
  //Call function for a page that is not found.
  //page fault
}
void set_page_as_free(int index){
  int LUT_offset= index%32;
  int LUT_index= index/32;
  lookuptable[LUT_index]=lookuptable[LUT_index]|(1<<LUT_offset);
  frametable[index].virtualAddress=-1;
  frametable[index].id=-1;
}
void wipe_thread_pages(tid_t id){
  int t = (int) id;
  int i;
  free_thread_id(id);
  for(i=0;i<page_limit;i++){
    if(frametable[i].id==t){
        //printf("FREED PAGE ADDRESS %d\n",*(int*)frametable[i].virtualAddress);
        //palloc_free_page(frametable[i].virtualAddress);
        set_page_as_free(i);
    }
  }
}

int page_fault_handler(tid_t id){
  //select random filled page
  random_init(0);
  int loop=0;
  int i;
  while(loop==0){
    i =random_ulong()%5;
    if(frametable[i].id!=-1)
      loop=1;
  }
  //printf("PAGE FAULT!!!!!!!!!!!!!!!!!!!VIRTUAL ADDRESS %d : %d : %d \n",frametable[i].virtualAddress,frametable[i].id,vtop(frametable[i].virtualAddress));
  //write that page to swap
  write_page_to_swap(frametable[i].virtualAddress,id);
  return i;
}
