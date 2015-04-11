#include <string.h>
#include "vm/frame.h"
#include <debug.h>
#include "kernel/palloc.c"
#include "kernel/vaddr.h"
#include <stdio.h>
#include <stdlib.h>

// sets every bit in the table to 1, meaning every slot is free
void preptable(){
  int i;
  for(i=0;i<16;i++){
    lookuptable[i]=-9223372036854775808;
  }
}

// finds the first empty slot, using ffsll, which returns the location of the first 1 bit in a given value
int find_empty_spot(){
  int i;
  for(i=0;i<16;i++){
    int t = ffsll(lookuptable[i]);
    if(t!=0){
      lookuptable[i]=lookuptable[i]&(~(1<<(t-1)));
      return i*64+t-1;
    }
  }
  PANIC("NO FREE FRAME");
  return 0;
}

// finds the page to evict, random choice at the moment, not going to be used yet
int find_evict_page(){
  srand(time(NULL));
  // get the page to evict
  int rand_cell = rand() % 16;
  int rand_bit = rand() % 64;
  // sets random bit to 1
  lookuptable[rand_cell] = lookuptable[rand_cell]|(1<<(randbit-1));
  return rand_cell*64+rand_bit-1;
}

// gets a new user page
void* acquire_user_page(int id){
  int index = find_empty_spot();
  frametable[index]->virtualAddress=palloc_get_page(PAL_USER);
  frametable[index]->t_id=id;


  //in process need to replace line 526 and 486 at least
  return frametable[index]->virtualAddress;
}
