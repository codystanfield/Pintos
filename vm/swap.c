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
  /*struct block *temp=block_first();
  while(temp!=NULL){
    if((*temp).type==BLOCK_SWAP){
      swap_device = temp;
    }
    temp = block_next(temp);
  }*/
  //printf("PAGSIZE:%u",PGSIZE);
  swap_device=block_get_role((enum block_type)BLOCK_SWAP);
  pageslots=block_size(swap_device)/8;
  swaptable=malloc((size_t)sizeof(ste) * (size_t)(pageslots));
  //printf("pageslots==%u\n",pageslots);
  lock_init(&lock);
  int i;
  for(i=0;i<pageslots;i++){
    swaptable[i].id=-1;
    swaptable[i].virtualAddress=-1;
  }
}

/* s_find_empty_slot parses the swap table and returns the location in the array
    of the first empty slot.*/

int s_find_empty_slot(){
  lock_acquire(&lock);
  uint32_t i;
  for(i=0;i<pageslots;i++){
    if(swaptable[i].id==-1){
      //printf("saving slot %d\n",i);
      swaptable[i].id=1;
      lock_release(&lock);
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

void write_page_to_swap(void* virtualAddress,size_t i){
  // because the swap slots are 8 times as big as the individual sectors, we multiply by 8 to get greater precision
  //intr_disable();
  //printf("MADE IT TO WRITE TO SWAP %u\n",virtualAddress); // debugging
  lock_acquire(&lock);
  block_write(swap_device,(block_sector_t)i*8+0,virtualAddress+(512*0));//(void*)vtop(virtualAddress));
  block_write(swap_device,(block_sector_t)i*8+1,virtualAddress+(512*1));//(void*)vtop(virtualAddress)+512);
  block_write(swap_device,(block_sector_t)i*8+2,virtualAddress+(512*2));//(void*)vtop(virtualAddress)+1024);
  block_write(swap_device,(block_sector_t)i*8+3,virtualAddress+(512*3));//(void*)vtop(virtualAddress)+1536);
  block_write(swap_device,(block_sector_t)i*8+4,virtualAddress+(512*4));//(void*)vtop(virtualAddress)+2048);
  block_write(swap_device,(block_sector_t)i*8+5,virtualAddress+(512*5));//(void*)vtop(virtualAddress)+2560);
  block_write(swap_device,(block_sector_t)i*8+6,virtualAddress+(512*6));//(void*)vtop(virtualAddress)+3072);
  block_write(swap_device,(block_sector_t)i*8+7,virtualAddress+(512*7));//(void*)vtop(virtualAddress)+3584);
  swaptable[i].id=1;
  lock_release(&lock);
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
    swaptable[i].id=-1;
    lock_release(&lock);

  /*return;
    }
  }
  PANIC("COULD NOT FIND PAGE");*/
}

// This is what I originally had, and what me might move to if we can get everything done.
// It uses a hash table instead of an array, so it would work better for out purposes,
// but it's more difficult to implement.

/*
#include "vm/swap.h"
#include "vm/page.h"
#include <debug.h>
#include "kernel/palloc.h"
#include "kernel/vaddr.h"
#include <stdio.h>
#include <stdlib.h>
#include "vm/frame.c"
#include "devices/block.h"
#include <hash.h> // thought it was "lib/kernel/hash.h", but PDF says otherwise (page 55)
#include <stdbool.h>

/* - We could use a hash table to store each of the elements in swap slots
   - Total size maybe 128 MB or less
   - To determine total size, at runtime, find total block size (at the moment it's set to 512 per block), then divide by 8
   - Lazily allocated
   - When contents of slot are read back to memory, or when process using slot is terminated, free slot
   - Page aligned */

// make a hash table
// make size of hash table 512 * # of blocks / 8
/*
bool swap_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
  const struct swap *a = hash_entry (a_, struct swap, hash_elem);
  const struct swap *b = hash_entry (b_, struct swap, hash_elem);
  return a->addr < b->addr;
} // taken from the PDF, then modified

// preps the swap table, will hash based on the address given
void prepswaptable()
{
  hash_init(&swap, hash_int, swap_less, NULL); // might be incorrect syntax
}

// finds the next empty slot in the swap table
int swap_insert(struct hash *hash, struct hash_elem *element) // returns 0 if successfull, nonzero otherwise
{
  struct hash_elem temp = hash_insert(hash, element);
  if (temp==NULL)
  {
    return 0;
  }
  return 1; // element already in hash
}

// frees the slot when process using the page is killed, or when the page gets moved back to memory
int free_slot(struct hash *hash, struct hash_elem *element)
{
  struct hash_elem temp = hash_delete(hash, element);
  if (temp == NULL)
  {
    return 1; // element not found
  }
  return 0;
}
*/
