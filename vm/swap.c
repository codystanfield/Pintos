#include "vm/swap.h"
#include "vm/page.h"
#include <debug.h>
#include "kernel/palloc.h"
#include "kernel/vaddr.h"
#include <stdio.h>
#include <stdlib.h>
#include "vm/frame.c"
#include "devices/block.h"
#include <hash.h> // thought it was "lib/kernel/hash.h", but PDF says otherwise (page 55)
#include <stdbool.h>

/* - We could use a hash table to store each of the elements in swap slots
   - Total size maybe 128 MB or less
   - To determine total size, at runtime, find total block size (at the moment it's set to 512 per block), then divide by 8
   - Lazily allocated
   - When contents of slot are read back to memory, or when process using slot is terminated, free slot
   - Page aligned */

// make a hash table
// make size of hash table 512 * # of blocks / 8

bool swap_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
  const struct swap *a = hash_entry (a_, struct swap, hash_elem);
  const struct swap *b = hash_entry (b_, struct swap, hash_elem);
  return a->addr < b->addr;
} // taken from the PDF, then modified

// preps the swap table, will hash based on the address given
void prepswaptable()
{
  hash_init(&swap, hash_int, swap_less, NULL); // might be incorrect syntax
}

// finds the next empty slot in the swap table
int swap_insert(struct hash *hash, struct hash_elem *element) // returns 0 if successfull, nonzero otherwise
{
  struct hash_elem temp = hash_insert(hash, element);
  if (temp==NULL)
  {
    return 0;
  }
  return 1; // element already in hash
}

// frees the slot when process using the page is killed, or when the page gets moved back to memory
int free_slot(struct hash *hash, struct hash_elem *element)
{
  struct hash_elem temp = hash_delete(hash, element);
  if (temp == NULL)
  {
    return 1; // element not found
  }
  return 0;
}
