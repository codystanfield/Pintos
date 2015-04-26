/*  Heavily influenced by the c vector implementation at
    http://www.happybearsoftware.com/implementing-a-dynamic-array.html

    Currently only works for (page_entry)s

    Dynamically grows, doubles in size when full 

    ----------------------------------------------------------------------------
    USAGE:
    Vector myVector;
    vector_init(&myVector, initial_size_of_vector);
    vector_append(&myVector, page);
    vector_set(&myVector, index, myPage);
    myPage = vector_get(&myVector, index);
    vector_free(&myVector);
    ----------------------------------------------------------------------------
*/
#include "vm/page.h"

typedef struct {
  int size;     //number of cells used
  int capacity; //total number of cells
  page_entry *pages;
} Vector;

void vector_init(Vector *vector, int capacity);
void vector_append(Vector *vector, page_entry *page);
page_entry vector_get(Vector *vector, int index);
void vector_set(Vector *vector, int index, page_entry *page);
void vector_double_capacity_if_full(Vector *vector);
void vector_free(Vector *vector);
