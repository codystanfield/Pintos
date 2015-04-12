#include "vm/frame.h"
#include <debug.h>
#include "kernel/palloc.h"
#include "kernel/vaddr.h"
#include <stdio.h>
#include "page.h"
int frameserved;
void preptable(){
  frameserved=0;
  int i;
  for(i=0;i<32;i++){
    lookuptable[i]=~0;
    //printf("%d\n",lookuptable[i]);
  }
}
int find_empty_spot(){
  int i;
  int t;
  for(i=0;i<32;i++){

    t= __builtin_ffsl(lookuptable[i]);
    if(t!=0){
      lookuptable[i]=lookuptable[i]&(~(1<<(t-1)));
      return i*32+t-1;
    }
    //printf("%d\n",t);
  }
  //printf("%d\n",t);
  PANIC("NO FREE FRAME");
  return 0;
}
void* aquire_user_page(int id,int zero,int stack){
  int index = find_empty_spot();
  frameserved+=1;
  if(zero==1)
    frametable[index].virtualAddress=palloc_get_page(PAL_USER);
  else
    frametable[index].virtualAddress=palloc_get_page(PAL_USER|PAL_ZERO);

  frametable[index].t_id=id;
  //printf("SERVeING FRAME NUMBER %d\n",frameserved);

  add_entry(index,id,4|stack);
  //in process need to replace line 526 and 486 at least
  return frametable[index].virtualAddress;
}

void free_user_page(void* page){
  int i;
  for(i=0;i<1024;i++){
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
  *(int*)frametable[index].virtualAddress=-1;
  frametable[index].t_id=-1;
}
void wipe_thread_pages(tid_t id){
  int t = (int) id;
  int i;
  free_thread_id(id);
  for(i=0;i<1024;i++){
    if(frametable[i].t_id==t){
        //printf("FREED PAGE ADDRESS %d\n",*(int*)frametable[i].virtualAddress);
        //palloc_free_page(frametable[i].virtualAddress);
        set_page_as_free(i);
    }
  }
}
