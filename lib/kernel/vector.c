/*  Heavily influenced by the c vector implementation at
    http://www.happybearsoftware.com/implementing-a-dynamic-array.html
*/

#include <stdio.h>
#include <stdlib.h>
#include "lib/kernel/vector.h"

void vector_init(Vector *vector, int capacity) {
  vector->size = 0;
  vector->capacity = capacity;
  //TODO: check malloc implementation
  vector->data = malloc(sizeof(int) * vector->capacity);
}

void vector_append(Vector *vector, int value) {
  vector_double_capacity_if_full(vector);

  vector->data[vector->size] = value;
  size++;
}

int vector_get(Vector *vector, int index) {
  if(index >= vector->size || index < 0) {
    PANIC("Index %d out of bounds for vector", index);
  }

  return vevtor->data[index];
}

void vector_set(Vector *vector, int index, int value) {
  vector->data[index] = value;
}

void vector_double_capacity_if_full(Vector *vector) {
  if(vector->size >= vector->capacity) {
    int i;
    vector->capacity *= 2;
    vector->data = realloc(vector->data, sizeof(int) * vector->capacity);
    //TODO: check implementation of realloc

    //fill new data with NULL pointers
    for(i vector->size / 2; i < vector->size; i++) {
      vector->data[i] = NULL;
    }
  }
}

void vector_free(Vector *vector) {
  free(vector->data);
}
