#include "vm/frame.h"
#include <debug.h>
#include "kernel/palloc.h"
#include "kernel/vaddr.h"
#include "devices/timer.h"
#include "lib/random.h"
#include "lib/limits.h"

/*  initialize the table to all 1s, signifying that all frames are free*/
void preptable(){
  int i;
  for(i=0;i<16;i++){
    lookuptable[i]= LLONG_MIN;
  }

  random_init(timer_elapsed(0));
}

/*  returns the first empty frame
    if no frame is empty, panics the kernel (for now)*/
int find_empty_spot(){
  int i;
  for(i=0;i<16;i++){
    int t;
    t= __builtin_ffsl(lookuptable[i]);
    if(t!=0){
      lookuptable[i]=lookuptable[i]&(~(1<<(t-1)));
      return i*64+t-1;
    }
  }
  // PANIC("NO FREE FRAME");

  // work in progress to free a page
  // //if no empty spot, free a page randomly (for now)
  int *cell;
  long long bit = evict_random_page(cell);
  lookuptable[*cell] = lookuptable[*cell] & ~(1 << bit);

  return 0;
}

/*  acquire a user page and update the frame table
    places the page in the first available empty spot*/
void* acquire_user_page(int id,int zero){
  int index = find_empty_spot();
  if(zero==1)
    frametable[index].virtualAddress=palloc_get_page(PAL_USER);
  else
    frametable[index].virtualAddress=palloc_get_page(PAL_USER|PAL_ZERO);

  frametable[index].t_id=id;


  //in process need to replace line 526 and 486 at least
  return frametable[index].virtualAddress;
}

/*  Work in progress to free a page*/
long long evict_random_page(int *cell) {
  *cell = (int) random_ulong() % 16;
  long long entry = (random_ulong() * 2) % LLONG_MAX;   //TODO: does the unsigned long need to be mutliplied by 2?

  lookuptable[*cell] = lookuptable[*cell] | (long long) (1 << entry); //TODO: do we need to cast?
  //TODO: need to actually swap the page, not just free it
  return entry;
}
