#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"
#include <list.h>

/* Starting and ending indices of the blocks in an inode */
#define FIRST_DIRECT_BLOCK 0
#define LAST_DIRECT_BLOCK 117
#define FIRST_INDIRECT_BLOCK (LAST_DIRECT_BLOCK + 1)
#define LAST_INDIRECT_BLOCK (FIRST_INDIRECT_BLOCK + 127)
#define FIRST_DOUBLY_INDIRECT_BLOCK (LAST_INDIRECT_BLOCK + 1)
#define LAST_DOUBLY_INDIRECT_BLOCK (FIRST_DOUBLY_INDIRECT_BLOCK + 16383)

#define NUM_DIRECT_BLOCKS (LAST_DIRECT_BLOCK + 1)
#define NUM_TOTAL_BLOCK (LAST_DOUBLY_INDIRECT_BLOCK + 1)

struct bitmap;

typedef struct {
  // uint32_t* direct_blocks[128];
  // block_sector_t* direct_blocks[128];
  block_sector_t direct_blocks[128];  //I think it would work to not use pointer: the values themselves are essentially the addresses of the sectors (I think)
} singly_indirect_block_;

typedef struct {
  singly_indirect_block_* singly_indirect_blocks[128];
} doubly_indirect_block_;

typedef struct {
  block_sector_t indirect_block_sectors[128];
} doubly_indirect_allocations_;

struct inode_disk {
	// block_sector_t start;               /* First data sector. */
	off_t length;                       /* File size in bytes. */
	unsigned magic;                     /* Magic number. */
	bool is_directory;

	block_sector_t parent_directory;
	block_sector_t singly_indirect_block_sector;
	block_sector_t doubly_indirect_block_sector;
	block_sector_t doubly_indirect_allocations_sector;
  
	block_sector_t direct_blocks[NUM_DIRECT_BLOCKS];	/* Array that holds sector numbers of direct blocks; addresses 63kB, or 63488 bytes */
	singly_indirect_block_* singly_indirect_block;	/* Pointer to block that contains array of pointers to direct blocks  (128 pointers total); addresses 64kB, or 65,536 bytes */
	doubly_indirect_block_* doubly_indirect_block;	/* Pointer to block that contains array of pointers to singly indirect blocks (128 pointers to singly indirect blocks, totals 16384 data blocks); addresses 8MB, or 8,388,608 bytes */
	doubly_indirect_allocations_* doubly_indirect_allocations;
};

/* In-memory inode. */
struct inode {
	struct list_elem elem;              /* Element in inode list. */
	block_sector_t sector;              /* Sector number of disk location. */
	int open_cnt;                       /* Number of openers. */
	bool removed;                       /* True if deleted, false otherwise. */
	int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
	struct inode_disk data;             /* Inode content. */
};

void inode_init (void);
bool inode_create (block_sector_t, off_t, bool);
struct inode* inode_open (block_sector_t);
struct inode* inode_reopen (struct inode*);
block_sector_t inode_get_inumber (const struct inode*);
void inode_close (struct inode*);
void inode_remove (struct inode*);
off_t inode_read_at (struct inode*, void*, off_t size, off_t offset);
off_t inode_write_at (struct inode*, const void*, off_t size, off_t offset);
void inode_deny_write (struct inode*);
void inode_allow_write (struct inode*);
off_t inode_length (const struct inode*);
// bool allocate_and_write(struct inode_disk*, )

#endif /* filesys/inode.h */
