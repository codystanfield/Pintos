#ifndef SWAP_H
#define SWAP_H

#include "vm/page.h"
struct swap{
  page page;
  int page_id;
  int t_id;
};
struct swap swaptable[1024];
void prepswaptable(void);
int find_empty_slot(void);
int free_slot(int id);

#endif
