#include "vm/swap.h"
#include "devices/block.h"
#include <debug.h>
#include "kernel/vaddr.h"
#include "kernel/malloc.h"
#include "kernel/interrupt.h"
void prepswaptable(){
  /*struct block *temp=block_first();
  while(temp!=NULL){
    if((*temp).type==BLOCK_SWAP){
      swap_device = temp;
    }
    temp = block_next(temp);
  }*/
  swap_device=block_get_role((enum block_type)BLOCK_SWAP);
  pageslots=block_size(swap_device)/8;
  swaptable=malloc((size_t)sizeof(ste) * (size_t)(pageslots));
  int i;
  for(i=0;i<pageslots;i++){
    swaptable[i].id=-1;
    swaptable[i].virtualAddress=-1;
  }
}

int s_find_empty_slot(){
  uint32_t i;
  for(i=0;i<pageslots;i++){
    if(swaptable[i].id==-1)
      return i;
  }
  PANIC("NO SWAP SLOT");
  return 0;
}

void write_page_to_swap(void* virtualAddress,tid_t id){
  int sector=8*s_find_empty_slot();
  //intr_disable();
  printf("MADE IT TO WRITE TO SWAP %d\n",virtualAddress);
  block_write(swap_device,(block_sector_t)sector+8,vtop(virtualAddress));//(void*)vtop(virtualAddress));
  /*printf("MADE IT PAST THE FIRST WRITE\n");
  block_write(swap_device,(block_sector_t)sector+1,virtualAddress+512);//(void*)vtop(virtualAddress)+512);
  block_write(swap_device,(block_sector_t)sector+2,virtualAddress+1024);//(void*)vtop(virtualAddress)+1024);
  block_write(swap_device,(block_sector_t)sector+3,virtualAddress+1536);//(void*)vtop(virtualAddress)+1536);
  block_write(swap_device,(block_sector_t)sector+4,virtualAddress+2048);//(void*)vtop(virtualAddress)+2048);
  block_write(swap_device,(block_sector_t)sector+5,virtualAddress+2560);//(void*)vtop(virtualAddress)+2560);
  block_write(swap_device,(block_sector_t)sector+6,virtualAddress+3072);//(void*)vtop(virtualAddress)+3072);
  block_write(swap_device,(block_sector_t)sector+7,virtualAddress+3584);//(void*)vtop(virtualAddress)+3584);
  */
  swaptable[sector/8].id=id;
  swaptable[sector/8].virtualAddress=virtualAddress;
  //intr_enable();
}

void read_page_from_swap(void* virtualAddress){
  uint32_t i;
  for(i=0;i<pageslots;i++){
    if(swaptable[i].virtualAddress==virtualAddress){
      block_read(swap_device,(block_sector_t)i*8,(void*)vtop(virtualAddress));
      block_read(swap_device,(block_sector_t)i*8+1,(void*)vtop(virtualAddress)+512);
      block_read(swap_device,(block_sector_t)i*8+2,(void*)vtop(virtualAddress)+1024);
      block_read(swap_device,(block_sector_t)i*8+3,(void*)vtop(virtualAddress)+1536);
      block_read(swap_device,(block_sector_t)i*8+4,(void*)vtop(virtualAddress)+2048);
      block_read(swap_device,(block_sector_t)i*8+5,(void*)vtop(virtualAddress)+2560);
      block_read(swap_device,(block_sector_t)i*8+6,(void*)vtop(virtualAddress)+3072);
      block_read(swap_device,(block_sector_t)i*8+7,(void*)vtop(virtualAddress)+3584);
    return;
    }
  }
  PANIC("COULD NOT FIND PAGE");
}
