#ifndef FRAME_H
#define FRAME_H

#include <string.h>
long long lookuptable[16];
struct fte{
  void* virtualAddress;
  int t_id; //thread id
};
struct fte frametable[1024];
void preptable(void);
int find_empty_spot(void);
void* acquire_user_page(int id,int zero);
long long evict_random_page(int *entry);
#endif
