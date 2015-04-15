#ifndef SWAP_H
#define SWAP_H

#include "vm/page.h"
#include <hash.h>
#include "devices/block.h"
struct swap{
  struct hash_elem hash_elem;
  page_entry page_entry;
  void *addr; // virtual address
  int page_id;
  int t_id;
};
struct hash swap;
void prepswaptable(void);
int swap_insert(void);
int free_slot(int id);

#endif
