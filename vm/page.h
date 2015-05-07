#ifndef PAGE_H
#define PAGE_H
#include "kernel/thread.h"
enum page_location{
  SWAP,
  FILE,
  ZERO,
  NONE
};
typedef struct {
  enum page_location loc;
  void* uaddr;
  bool writeable;
  bool loaded;
  short frame_index;
  uint32_t* pagedir;
  size_t swap_index;
  struct{
    struct file* file;
    off_t ofs;
    size_t read_bytes;
    size_t zero_bytes;
  } file;

}Page;

void preppagetable(void);
Page* file_page(struct file *file, off_t ofs,size_t read_bytes, size_t zero_bytes, bool writeable, uint8_t* upage);
Page* zero_page(void* addr,bool writable);
bool load_page(Page* page, bool lock);
void load_from_file(void* kpage,Page* page);
void printpagestats(Page* page);
#endif
