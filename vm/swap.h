#ifndef SWAP_H
#define SWAP_H

#include <inttypes.h>
#include <stddef.h>
#include "kernel/thread.h"
#include "devices/block.h"

struct block* swap_device;

typedef struct {
  bool free;
} ste; // struct for swap table entries

ste *swaptable;
block_sector_t pageslots;


// preparing the functions

void prepswaptable(void);
int s_find_empty_slot(void);
int write_page_to_swap(void* virtualAddress);
void read_page_from_swap(size_t i,void* virtualAddress);

#endif
