/*  Heavily influenced by the c vector implementation at
    http://www.happybearsoftware.com/implementing-a-dynamic-array.html

    Currently only works for (page_entry)s
*/

#include <stdio.h>
#include <stdlib.h>
#include "lib/kernel/vector.h"
#include "kernel/malloc.h"

/*  Run before using a new vector
    sets current size to 0 and capacity to provided capacity
    allocates memory to hold the pages */
void vector_init(Vector *vector, int capacity) {
  vector->size = 0;
  vector->capacity = capacity;
  //TODO: check malloc implementation
  vector->pages = malloc(sizeof(page_entry) * vector->capacity);
}

/*  Append a page to the end of the vector
    If the vector is full, double the size */
void vector_append(Vector *vector, page_entry *page) {
  vector_double_capacity_if_full(vector);

  vector->pages[vector->size] = *page;
  vector->size++;
}

/*  Returns the page at the given index
    If the index is out of bounds, panics the kernel */
page_entry vector_get(Vector *vector, int index) {
  if(index >= vector->size || index < 0) {
    PANIC("Trying to get index %d - out of bounds for vector\n", index);
  }

  return vector->pages[index];
}

/*  Set the index in the pages array (not really an array, but treated as one)
    to the supplied page
    If the index is out of bounds, panics the kernel */
void vector_set(Vector *vector, int index, page_entry *page) {
  if(index >= vector->size || index < 0) {
    PANIC("Trying to set index %d - out of bounds for vector\n", index);
  }
  vector->pages[index] = *page;
}

/*  Called when appending
    Doubles the capacity of the vector if the vector is full
    Fills the new space with NULL */
void vector_double_capacity_if_full(Vector *vector) {
  if(vector->size >= vector->capacity) {
    int i;
    vector->capacity *= 2;
    vector->pages = realloc(vector->pages, sizeof(page_entry) * vector->capacity);
    //TODO: check implementation of realloc

    //fill new data with NULL pointers
    for(i = vector->size; i < vector->capacity; i++) {
      vector_append(vector, NULL);
    }
    //need to reset size after appending NULLs
    vector->size = vector->capacity / 2;
  }
}

/*  Frees the memory allocated to the vector */
void vector_free(Vector *vector) {
  free(vector->pages);
}
