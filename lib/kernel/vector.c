/*  Heavily influenced by the c vector implementation at
    http://www.happybearsoftware.com/implementing-a-dynamic-array.html

    Currently only works for (page_entry)s
*/

#include <stdio.h>
#include <stdlib.h>
#include "lib/kernel/vector.h"
#include "kernel/malloc.h"

void vector_init(Vector *vector, int capacity) {
  vector->size = 0;
  vector->capacity = capacity;
  //TODO: check malloc implementation
  vector->pages = malloc(sizeof(page_entry) * vector->capacity);
}

void vector_append(Vector *vector, page_entry *page) {
  vector_double_capacity_if_full(vector);

  vector->pages[vector->size] = *page;
  vector->size++;
}

page_entry vector_get(Vector *vector, int index) {
  if(index >= vector->size || index < 0) {
    PANIC("Index %d out of bounds for vector", index);
  }

  return vector->pages[index];
}

void vector_set(Vector *vector, int index, page_entry *page) {
  vector->pages[index] = *page;
}

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

void vector_free(Vector *vector) {
  free(vector->pages);
}
