#ifndef SWAP_H
#define SWAP_H

#include <inttypes.h>
#include <stddef.h>
#include "kernel/thread.h"
#include "devices/block.h"

struct block* swap_device;

bool* swaptable;
block_sector_t pageslots;



void prepswaptable(void);
int s_find_empty_slot(void);
void swap_set_free(int i);
int write_page_to_swap(void* virtualAddress);
void read_page_from_swap(size_t i,void* virtualAddress);
void printswapstats(void);

#endif
