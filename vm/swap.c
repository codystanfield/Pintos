#include "vm/swap.h"
#include "vm/page.c"
#include <debug.h>
#include "kernel/palloc.h"
#include "kernel/vaddr.h"
#include <stdio.h>
#include <stdlib.h>
#include "vm/frame.c"

// finds the next empty slot in the swap table
int find_empty_slot()
{

  return 0;
}

// frees the slot when process using the page is killed, or when the page gets moved back to memory
int free_slot(/*slot id*/)
{

  return 0;
}
