#include <string.h>
#include "vm/frame.h"
#include <debug.h>
#include "kernel/palloc.c"
#include "kernel/vaddr.h"

void preptable(){
  int i;
  for(i=0;i<16;i++){
    lookuptable[i]=-9223372036854775808;
  }
}
int find_empty_spot(){
  int i;
  for(i=0;i<16;i++){
    int t = ffsll(lookuptable[i]);
    if(t!=0){
      lookuptable[i]=lookuptable[i]&(~(1<<(t-1)));
      return i*64+t-1;
    }
  }
  PANIC("NO FREE FRAME");
  return 0;
}
void* aquire_user_page(int id){
  int index = find_empty_spot();
  frametable[index]->virtualAddress=palloc_get_page(PAL_USER);
  frametable[index]->t_id=id;


  //in process need to replace line 526 and 486 at least
  return frametable[index]->virtalAddress;
}
