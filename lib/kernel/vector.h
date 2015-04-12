/*  Heavily influenced by the c vector implementation at
    http://www.happybearsoftware.com/implementing-a-dynamic-array.html
*/
#include "vm/page.h"

//TODO: make sure to request an initial size

typedef struct {
  int size;     //number of cells used
  int capacity; //total number of cells
  struct page_entry *data;    //pointer to data TODO: should it be an int pointer?
} Vector;

void vector_init(Vector *vector, int capacity);
void vector_append(Vector *vector, int value);
int vector_get(Vector *vector, int index);
void vector_set(Vector *vector, int index, int value);
void vector_double_capacity_if_full(Vector *vector);
void vector_free(Vector *vector);
