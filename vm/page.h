#ifndef PAGE_H
#define PAGE_H
#include "kernel/thread.h"
//#include "lib/kernel/vector.h"
enum page_location{
  SWAP,
  FILE,
  ZERO,
  NONE
};
typedef struct {
  enum page_location loc;
  bool writeable;
  bool loaded;
  void* uaddr;
  void* kpage;
  int frame_index;
  uint32_t* pagedir;
  size_t swap_index;
  bool zeroed;
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
void add_page(Page* page);
void add_page(Page* page);
bool load_from_file(void* kpage,Page* page);
#endif
