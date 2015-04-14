#ifndef SWAP_H
#define SWAP_H
#include <inttypes.h>
#include <stddef.h>
#include "kernel/thread.h"
#include "devices/block.h"
struct block* swap_device;
typedef struct {
  tid_t id;
  void* virtualAddress;
}ste;
ste *swaptable;
block_sector_t pageslots;

void prepswaptable(void);
int find_empty_slot(void);
void write_page_to_swap(void* virtualAddress,tid_t id);
void read_page_from_swap(void* virtualAddress);
#endif
