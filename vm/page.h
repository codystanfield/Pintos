#ifndef PAGE_H
#define PAGE_H

struct {
    /* The attributes of the attr are based on the bits flipped on or of.
       The least sig bit represents if it is a stack page. Next bit is for
       swap and the next is for if it is in a frame.*/
    char attr;
    int location;
} page_entry;

// dynamic array of pages.
