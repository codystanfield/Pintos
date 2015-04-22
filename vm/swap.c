#include "vm/swap.h"
#include "devices/block.h"
#include <debug.h>
#include "kernel/vaddr.h"
#include "kernel/malloc.h"
#include "kernel/interrupt.h"
#include "kernel/synch.h"

/* prepswaptable is a void function that gets called at startup, and it, as the name suggests,
   preps the swap table by initializing the array of structs that will be used. */
struct lock lock;
void prepswaptable(){
  lock_init(&lock);
  lock_acquire(&lock);
  swap_device=block_get_role((enum block_type)BLOCK_SWAP);
  pageslots=block_size(swap_device)/8;
  swaptable=malloc((size_t)sizeof(ste) * (size_t)(pageslots));
  //printf("pageslots==%u\n",pageslots)
  unsigned i;
  for(i=0;i<pageslots;i++){
    swaptable[i].free=true;
  }
  lock_release(&lock);
}

/* s_find_empty_slot parses the swap table and returns the location in the array
    of the first empty slot.*/

int s_find_empty_slot(){
  uint32_t i;
  for(i=0;i<pageslots;i++){
    if(swaptable[i].free){
      //printf("saving slot %d\n",i);
      swaptable[i].free=false;
      return i;
    }
  }
  PANIC("NO SWAP SLOT");
  return 0;
}

/* write_page_to_swap takes the virtual address of a page that you want written to swap
   and the thread id so that you can assign the new addition to swap its id.

   There are 8 calls to block_write because sectors are 8 times as small a block, so
   this way it gets written all at once to a single, contiguous piece of memory.*/

int write_page_to_swap(void* virtualAddress){
  // because the swap slots are 8 times as big as the individual sectors, we multiply by 8 to get greater precision
  //intr_disable();
  //printf("MADE IT TO WRITE TO SWAP %u\n",virtualAddress); // debugging
  lock_acquire(&lock);
  int i = s_find_empty_slot();
  block_write(swap_device,(block_sector_t)i*8+0,virtualAddress+(512*0));//(void*)vtop(virtualAddress));
  block_write(swap_device,(block_sector_t)i*8+1,virtualAddress+(512*1));//(void*)vtop(virtualAddress)+512);
  block_write(swap_device,(block_sector_t)i*8+2,virtualAddress+(512*2));//(void*)vtop(virtualAddress)+1024);
  block_write(swap_device,(block_sector_t)i*8+3,virtualAddress+(512*3));//(void*)vtop(virtualAddress)+1536);
  block_write(swap_device,(block_sector_t)i*8+4,virtualAddress+(512*4));//(void*)vtop(virtualAddress)+2048);
  block_write(swap_device,(block_sector_t)i*8+5,virtualAddress+(512*5));//(void*)vtop(virtualAddress)+2560);
  block_write(swap_device,(block_sector_t)i*8+6,virtualAddress+(512*6));//(void*)vtop(virtualAddress)+3072);
  block_write(swap_device,(block_sector_t)i*8+7,virtualAddress+(512*7));//(void*)vtop(virtualAddress)+3584);
  lock_release(&lock);
  return i;
  //intr_enable();
}

/* read_page_from_swap takes the virtual address of a page in the swap space and uses it
   to compare to everything in the swap space, trying to find the correct page.

   It operates similar to write_page_to_swap, in that it calls the block_ function 8
   times so that it can get the entire block from the swap space.

   At the moment, it isn't very efficient, but we'll optimize it once we have everything
   working properly. We might use a hash table if we get the chance. */

void read_page_from_swap(size_t i,void* virtualAddress){
  /*uint32_t i;
  for(i=0;i<pageslots;i++){
    if(swaptable[i].virtualAddress==virtualAddress){
    */
    lock_acquire(&lock);
    block_read(swap_device,(block_sector_t)i*8+0,virtualAddress+(512*0));
    block_read(swap_device,(block_sector_t)i*8+1,virtualAddress+(512*1));
    block_read(swap_device,(block_sector_t)i*8+2,virtualAddress+(512*2));
    block_read(swap_device,(block_sector_t)i*8+3,virtualAddress+(512*3));
    block_read(swap_device,(block_sector_t)i*8+4,virtualAddress+(512*4));
    block_read(swap_device,(block_sector_t)i*8+5,virtualAddress+(512*5));
    block_read(swap_device,(block_sector_t)i*8+6,virtualAddress+(512*6));
    block_read(swap_device,(block_sector_t)i*8+7,virtualAddress+(512*7));
    swaptable[i].free=true;
    lock_release(&lock);

  /*return;
    }
  }
  PANIC("COULD NOT FIND PAGE");*/
}
