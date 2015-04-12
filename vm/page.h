#ifndef PAGE_H
#define PAGE_H
#include "kernel/thread.h"
//#include "lib/kernel/vector.h"
typedef struct {
    /* The attributes of the attr are based on the bits flipped on or of.
       The least sig bit represents if it is a stack page. Next bit is for
       swap and the next is for if it is in a frame.
       00000001 -stack page
       00000010 -swap page
       00000100 -frame page
       */
    int location;
    tid_t id;
    char attr;

} page_entry;

page_entry sup_page_table[512];
// dynamic array of pages.

void preppagetable(void);
int find_empty_slot(void);
void add_entry(int location,tid_t id,char attr);
void free_thread_id(tid_t id);
#endif
