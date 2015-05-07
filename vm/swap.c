#include "vm/swap.h"
#include "devices/block.h"
#include <debug.h>
#include <stdio.h>
#include "kernel/vaddr.h"
#include "kernel/malloc.h"
#include "kernel/interrupt.h"
#include "kernel/synch.h"

/* prepswaptable is a void function that gets called at startup, and it, as the name suggests,
   preps the swap table by initializing the array of bool that will be used. The semantics are
   as follows. If and index is true the it is free to use. If not it is taken.*/
static struct lock swap_lock;
void prepswaptable(){
  lock_init(&swap_lock);
  swap_device=block_get_role((enum block_type)BLOCK_SWAP);
  pageslots=block_size(swap_device)/8;
  swaptable=malloc((size_t)sizeof(bool) * (size_t)(pageslots));
  unsigned i;
  for(i=0;i<pageslots;i++){
    swaptable[i]=true;
  }
}

/*Finds an empty slot for the write function*/

int s_find_empty_slot(){
  uint32_t i;
  for(i=0;i<pageslots;i++){
    if(swaptable[i]){
      swaptable[i]=false;
      return i;
    }
  }
  PANIC("NO SWAP SLOT");
  return 0;
}
/*Sets the swap slot at index i to free. Should
  only ever be called during the destruction of
  a processes pagedir*/
void swap_set_free(int i){
  lock_acquire(&swap_lock);
  swaptable[i]=true;
  lock_release(&swap_lock);
}
/*Debuging function to make sure swap slots were being
  free'd on process exit*/
void printswapstats(){
  lock_acquire(&swap_lock);
  unsigned i;
  int totalf=0;
  int totalt=0;
  for(i=0;i<pageslots;i++){
    if(swaptable[i])
      totalf++;
    else
      totalt++;
  }
  printf("FREE SLOTS==%d  TAKEN SLOTS==%d\n",totalf,totalt);
  lock_release(&swap_lock);
}


/*Write a page start at address virtualAddress to
  a swap slot*/
int write_page_to_swap(void* virtualAddress){
  lock_acquire(&swap_lock);
  int i = s_find_empty_slot();
  block_write(swap_device,(block_sector_t)i*8+0,virtualAddress+(512*0));//(void*)vtop(virtualAddress));
  block_write(swap_device,(block_sector_t)i*8+1,virtualAddress+(512*1));//(void*)vtop(virtualAddress)+512);
  block_write(swap_device,(block_sector_t)i*8+2,virtualAddress+(512*2));//(void*)vtop(virtualAddress)+1024);
  block_write(swap_device,(block_sector_t)i*8+3,virtualAddress+(512*3));//(void*)vtop(virtualAddress)+1536);
  block_write(swap_device,(block_sector_t)i*8+4,virtualAddress+(512*4));//(void*)vtop(virtualAddress)+2048);
  block_write(swap_device,(block_sector_t)i*8+5,virtualAddress+(512*5));//(void*)vtop(virtualAddress)+2560);
  block_write(swap_device,(block_sector_t)i*8+6,virtualAddress+(512*6));//(void*)vtop(virtualAddress)+3072);
  block_write(swap_device,(block_sector_t)i*8+7,virtualAddress+(512*7));//(void*)vtop(virtualAddress)+3584);
  lock_release(&swap_lock);
  return i;
}

/*Reads a page from swap index i into the frame at address
  virtualAddress*/
void read_page_from_swap(size_t i,void* virtualAddress){
  lock_acquire(&swap_lock);
  block_read(swap_device,(block_sector_t)i*8+0,virtualAddress+(512*0));
  block_read(swap_device,(block_sector_t)i*8+1,virtualAddress+(512*1));
  block_read(swap_device,(block_sector_t)i*8+2,virtualAddress+(512*2));
  block_read(swap_device,(block_sector_t)i*8+3,virtualAddress+(512*3));
  block_read(swap_device,(block_sector_t)i*8+4,virtualAddress+(512*4));
  block_read(swap_device,(block_sector_t)i*8+5,virtualAddress+(512*5));
  block_read(swap_device,(block_sector_t)i*8+6,virtualAddress+(512*6));
  block_read(swap_device,(block_sector_t)i*8+7,virtualAddress+(512*7));
  swaptable[i]=true;
  lock_release(&swap_lock);
}
