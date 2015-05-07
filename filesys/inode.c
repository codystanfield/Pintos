#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include <stdio.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "kernel/malloc.h"


/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
// struct inode_disk {
// 	// block_sector_t start;               /* First data sector. */
// 	off_t length;                       /* File size in bytes. */
// 	unsigned magic;                     /* Magic number. */
// 	bool is_directory;
// 	block_sector_t parent_directory;
// 	block_sector_t singly_indirect_block_sector;
// 	block_sector_t doubly_indirect_block_sector;
// 	block_sector_t doubly_indirect_allocations_sector;
// 	// uint32_t* direct_blocks[123];						/* Array of pointers to direct blocks; addresses 61.5kB, or 62,976 bytes */
// 	// block_sector_t* direct_blocks[124];	/* Array of pointers to direct blocks; addresses 63kB, or 63488 bytes */
// 	block_sector_t direct_blocks[NUM_DIRECT_BLOCKS];	/* Array that holds sector numbers of direct blocks; addresses 63kB, or 63488 bytes */
// 	singly_indirect_block_* singly_indirect_block;	/* Pointer to block that contains array of pointers to direct blocks  (128 pointers total); addresses 64kB, or 65,536 bytes */
// 	doubly_indirect_block_* doubly_indirect_block;	/* Pointer to block that contains array of pointers to singly indirect blocks (128 pointers to singly indirect blocks, totals 16384 data blocks); addresses 8MB, or 8,388,608 bytes */
// 	doubly_indirect_allocations_* doubly_indirect_allocations;
// 	// singly_indirect_block_ singly_indirect_block;	/* Pointer to block that contains array of pointers to direct blocks  (128 pointers total); addresses 64kB, or 65,536 bytes */
// 	// doubly_indirect_block_ doubly_indirect_block;	/* Pointer to block that contains array of pointers to singly indirect blocks (128 pointers to singly indirect blocks, totals 16384 data blocks); addresses 8MB, or 8,388,608 bytes */
// };
//
// /* In-memory inode. */
// struct inode {
// 	struct list_elem elem;              /* Element in inode list. */
// 	block_sector_t sector;              /* Sector number of disk location. */
// 	int open_cnt;                       /* Number of openers. */
// 	bool removed;                       /* True if deleted, false otherwise. */
// 	int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
// 	struct inode_disk data;             /* Inode content. */
// };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long.
	 Note: starts at 1, not 0, so usually going to need to subtract 1 from
	 result to get index (since the file is still viewed as being contiguous) */
static inline size_t
bytes_to_sectors (off_t size) {
	return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
	 If allocate_if_not_found is set to true, will allocate a sector to the given
	 location, but will NOT write the sector to the disk or allocate any sectors
	 other than at the position requested. The calling function	 will be
	 responsible for the write and any other allocations needed.
   If allocate_if_not_found is set to falseReturns -1 if INODE does not contain
	 data for a byte at offset POS.
	 If allocating blocks, this function will also write them to the disk. This is
	 bad for performance, but much easier on the programmer to not have to worry
	 about what all has been allocated
	 Also, some of this code (such as the if/else blocks) is inneficient, but again,
	 readability and ease of use too precedence */
	//NOTE: this used to accept a const struct inode*, but I want to be able to change it. I think this is OK
	//TODO: make sure to come back and check this
static block_sector_t byte_to_sector (struct inode* inode, off_t pos, bool allocate_if_not_found) {
	// //printf("byte_to_sector on inode->sector %d, offset is %d\n", inode->sector, pos);
	static int count = 1;
	//////printf("byte_to_sector called %d times\n", count++);
	//////printf("in byte_to_sector, inode is %d, offset is %d, inode length is %d\n", inode->sector, pos, inode->data.length);
	ASSERT (inode != NULL);
	//////printf("Now past the assertion\n");

	/* If accessing a byte out of range and don't want to allocate, return -1 */
	if(pos >= inode->data.length && !allocate_if_not_found)
		return -1;
	// //printf("Past first if\n");

	/* If the requested byte is within the file size, find the corresponding sector */
	block_sector_t sector_idx = pos / BLOCK_SECTOR_SIZE;
	int section;	//0 = direct, 1 = indirect, 2 = doubly indirect
	static char zeros[BLOCK_SECTOR_SIZE];
	block_sector_t new_sector = -1;
	int i;

	if(sector_idx < FIRST_INDIRECT_BLOCK)
		section = 0;
	else if(sector_idx < FIRST_DOUBLY_INDIRECT_BLOCK)
		section = 1;
	else if(sector_idx < LAST_DOUBLY_INDIRECT_BLOCK + 1)
		section = 2;
	else {
		PANIC("In byte_to_sector, sector_idx out of bounds, likely just going to return -1, check this\n");
		return -1;
	}

	// //printf("section is %d\n", section);
	//////printf("section is %d\n", section);
	//////printf("pos is %d\n", pos);
	//////printf("FIRST_INDIRECT_BLOCK_IS %d\n", FIRST_INDIRECT_BLOCK);

	switch(section) {
		/* In the direct blocks */
		case 0: {
			if(inode->data.direct_blocks[sector_idx] != -1) {
				// //printf("case 0, in if\n");
				return inode->data.direct_blocks[sector_idx];
			}
			else if(allocate_if_not_found) {
				// //printf("case 0, in else if\n");
				free_map_allocate(1, &new_sector);
				inode->data.direct_blocks[sector_idx] = new_sector;
				block_write(fs_device, inode->sector, &inode->data);
				block_write(fs_device, new_sector, zeros);

				return new_sector;
			}
			else {
				// //printf("case 0, in else\n");
				return -1;
			}

			break;
		}

		/* In the singly indirect block */
		case 1: {
			int direct_index = sector_idx - FIRST_INDIRECT_BLOCK;

			/* If not allocating */
			if(!allocate_if_not_found) {
				if(inode->data.singly_indirect_block_sector == -1)
					return -1;
				return inode->data.singly_indirect_block->direct_blocks[direct_index];
			}
			/* If allocating */
			else {
				/* Allocate the sectors */
				if(inode->data.singly_indirect_block_sector == -1) {
					/* Allocate the indirect block */
					free_map_allocate(1, &new_sector);
					inode->data.singly_indirect_block_sector = new_sector;
					inode->data.singly_indirect_block = calloc(1, BLOCK_SECTOR_SIZE);

					/* Initialize the indirect block */
					for(i = 0; i < 128; i++) {
						inode->data.singly_indirect_block->direct_blocks[i] = -1;
					}

					/* Write to disk */
					block_write(fs_device, new_sector, inode->data.singly_indirect_block);
					block_write(fs_device, inode->sector, &inode->data);
				}
				if(inode->data.singly_indirect_block->direct_blocks[direct_index] == -1) {
					free_map_allocate(1, &new_sector);
					inode->data.singly_indirect_block->direct_blocks[direct_index] = new_sector;
					block_write(fs_device, new_sector, zeros);
					block_write(fs_device, inode->data.singly_indirect_block_sector, inode->data.singly_indirect_block);
				}

				return inode->data.singly_indirect_block->direct_blocks[direct_index];
			}

			break;
		}

		/* In the doubly indirect block */
		//TODO: Writing to block still wrong!!!
		case 2: {
			int indirect_index = (sector_idx - FIRST_DOUBLY_INDIRECT_BLOCK) / 128;
			int direct_index = (sector_idx - FIRST_DOUBLY_INDIRECT_BLOCK) % 128;

			/* If not allocating */
			if(!allocate_if_not_found) {
				if(inode->data.doubly_indirect_block_sector == -1)
					return -1;
				if(inode->data.doubly_indirect_allocations->indirect_block_sectors[indirect_index] == -1)
					return -1;
				return inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index]->direct_blocks[direct_index];
			}
			/* If allocating */
			else {
				/* Allocate the sectors */
				if(inode->data.doubly_indirect_block_sector == -1) {
					/* Allocate the doubly indirect block */
					free_map_allocate(1, &new_sector);
					inode->data.doubly_indirect_block_sector = new_sector;
					inode->data.doubly_indirect_block = calloc(1, BLOCK_SECTOR_SIZE);

					/* Allocate the doubly indirect allocations block */
					free_map_allocate(1, &new_sector);
					inode->data.doubly_indirect_allocations_sector = new_sector;
					inode->data.doubly_indirect_allocations = calloc(1, BLOCK_SECTOR_SIZE);

					/* Initialize the doubly indirect block and doubly indirect allocations block */
					for(i = 0; i < 128; i++) {
						inode->data.doubly_indirect_block->singly_indirect_blocks[i] = NULL;
						inode->data.doubly_indirect_allocations->indirect_block_sectors[i] = -1;
					}
				}
				if(inode->data.doubly_indirect_allocations->indirect_block_sectors[indirect_index] == -1) {
					/* Allocate the indirect block needed */
					free_map_allocate(1, &new_sector);
					inode->data.doubly_indirect_allocations->indirect_block_sectors[indirect_index] = new_sector;
					inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index] = calloc(1, BLOCK_SECTOR_SIZE);

					/* Initialize the indirect block */
					for(i = 0; i < 128; i++) {
						inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index]->direct_blocks[direct_index] = -1;
					}
				}
				if(inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index]->direct_blocks[direct_index] == -1) {
					free_map_allocate(1, &new_sector);
					inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index]->direct_blocks[direct_index] = new_sector;
				}

				//TODO: Shouldn't write if found dammit! Maybe it's ok though (other than the zeros one)....
				/* Write the sectors */
				block_write(fs_device, inode->sector, &inode->data);
				block_write(fs_device, inode->data.doubly_indirect_block_sector, inode->data.doubly_indirect_block);
				block_write(fs_device, inode->data.doubly_indirect_allocations->indirect_block_sectors[indirect_index], inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index]);
				block_write(fs_device, inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index]->direct_blocks[direct_index], zeros);

				return inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index]->direct_blocks[direct_index];
			}


			// //TODO: I just checked these, but check again
			// int indirect_index = (sector_idx - FIRST_DOUBLY_INDIRECT_BLOCK) / 128;
			// int direct_index = (sector_idx - FIRST_DOUBLY_INDIRECT_BLOCK) % 128;
			//
			// /* If the doubly indirect block is allocated */
			// // if(inode->data.doubly_indirect_block) {
			// if(inode->data.doubly_indirect_block_sector != -1) {
			// 	if(inode->data.doubly_indirect_allocations->indirect_block_sectors[indirect_index] != -1)
			// 		return inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index]->direct_blocks[direct_index];
			// 	else if(allocate_if_not_found) {
			// 		inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index] = calloc(1, BLOCK_SECTOR_SIZE);
			// 		free_map_allocate(1, &inode->data.doubly_indirect_allocations->indirect_block_sectors[indirect_index]);
			// 		free_map_allocate(1, &inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index]->direct_blocks[direct_index]);
			// 		return inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index]->direct_blocks[direct_index];
			// 	}
			// 	else
			// 		return -1;
			// }
			// /* If the doubly indirect block is not allocated and you want to allocate it */
			// else if(allocate_if_not_found) {
			// 	inode->data.doubly_indirect_block = calloc(1, BLOCK_SECTOR_SIZE);
			// 	inode->data.doubly_indirect_allocations = calloc(1, BLOCK_SECTOR_SIZE);
			// 	inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index] = calloc(1, BLOCK_SECTOR_SIZE);
			// 	free_map_allocate(1, &inode->data.doubly_indirect_block_sector);
			// 	free_map_allocate(1, &inode->data.doubly_indirect_allocations_sector);
			// 	free_map_allocate(1, &inode->data.doubly_indirect_allocations->indirect_block_sectors[indirect_index]);
			// 	free_map_allocate(1, &inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index]->direct_blocks[direct_index]);
			// 	return inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index]->direct_blocks[direct_index];
			// }
			// else
			// 	return -1;

			break;
		}

		default: {
			//TODO: should I panic or return -1? Probably just gonna stick with return for now
			PANIC("May want to check this, in byte_to_sector, sector out of bounds. Should probably just return -1\n");
			return -1;
		}
	}
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) {
	list_init (&open_inodes);
}

//----------------------------ORIGINAL------------------------
/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
//TODO: need to update for new inode structure. Make sure to have the pointers point to NULL to keep file size down while you can
//TODO: also need to allow for a default value to be created
// bool
// inode_create (block_sector_t sector, off_t length) {
// 	////////////printf("In inode_create\n");
// 	struct inode_disk* disk_inode = NULL;
// 	bool success = false;
//
// 	ASSERT (length >= 0);
//
// 	/* If this assertion fails, the inode structure is not exactly
// 	   one sector in size, and you should fix that. */
// 	ASSERT (sizeof * disk_inode == BLOCK_SECTOR_SIZE);
//
// 	disk_inode = calloc (1, sizeof * disk_inode);
// 	if (disk_inode != NULL) {
// 		size_t sectors = bytes_to_sectors (length);
// 		disk_inode->length = length;
// 		disk_inode->magic = INODE_MAGIC;
// 		if (free_map_allocate (sectors, &disk_inode->start)) {
// 			block_write (fs_device, sector, disk_inode);
// 			if (sectors > 0) {
// 				static char zeros[BLOCK_SECTOR_SIZE];
// 				size_t i;
// 				//TODO: I think this initializes the pointers to NULL (since everything is zeroed out)
// 				//This actually seems like it will zero out the disk one sector at a time
// 				for (i = 0; i < sectors; i++) {
// 					block_write (fs_device, disk_inode->start + i, zeros);
// 				}
// 			}
// 			success = true;
// 		}
// 		free (disk_inode);
// 	}
// 	return success;
// }

//----------------------------FIRST TRY------------------------
/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
//TODO: need to update for new inode structure. Make sure to have the pointers point to NULL to keep file size down while you can
//TODO: also need to allow for a default value to be created
/*Much of this is similar to byte_to_sector, but it would be a waste to call
  byte_to_sector because no sectors have been allocated yet
	TODO: COULD pass another variable in to bytes_to_sector, like
	allocate_if_not_found...*/
// bool
// inode_create (block_sector_t sector, off_t length, bool is_dir) {
// 	////////////printf("In inode_create\n");
// 	struct inode_disk* disk_inode = NULL;
// 	bool success = false;		//TODO: need to actually use this...
// 	block_sector_t new_sector;
// 	static char zeros[BLOCK_SECTOR_SIZE];
// 	int i, j;
//
// 	////////////printf("ic 1\n");
// 	ASSERT(length >= 0);
//
// 	/* If this assertion fails, the inode structure is not exactly
// 	one sector in size, and you should fix that. */
// 	ASSERT(sizeof * disk_inode == BLOCK_SECTOR_SIZE);
// 	////////////printf("ic 2\n");
//
// 	disk_inode = calloc(1, sizeof * disk_inode);
// 	////////////printf("ic 2.5\n");
// 	if(disk_inode != NULL) {	//If allocation of memory was successful
// 		////////////printf("ic 3\n");
// 		size_t sectors = bytes_to_sectors(length);
// 		block_sector_t sector_idx = sectors - 1;	//Not strictly necessary, but makes it much easier on the programmer since we usually work with the index, not the number of sectors
// 		int section;
// 		disk_inode->length = length;
// 		disk_inode->magic = INODE_MAGIC;
// 		////////////printf("ic 4\n");
// 		disk_inode->is_directory = is_dir ? true : false;
// 		////////////printf("ic 5\n");
//
// 		if(sector_idx < FIRST_INDIRECT_BLOCK)
// 			section = 0;
// 		else if(sector_idx < FIRST_DOUBLY_INDIRECT_BLOCK)
// 			section = 1;
// 		else if(sector_idx < LAST_DOUBLY_INDIRECT_BLOCK + 1)
// 			section = 2;
// 		else
// 			PANIC("Assuming calculations are correct, trying to create a file larger than max file size\n");
//
// 		//If creating an empty file
// 		if(sectors == 0) {
// 			////////////printf("ic 6\n");
// 			for(i = 0; i < NUM_INDIRECT_BLOCKS; i++) {
// 				disk_inode->direct_blocks[i] = -1;	//TODO: REMEMBER!!! This is NOT -1! it is actually the max representable 32 bit unsigned int
// 				////////////printf("HELLLLOOOOO!!!!\t%u\n", disk_inode->direct_blocks[i]);
// 			}
// 			disk_inode->singly_indirect_block = NULL;
// 			disk_inode->doubly_indirect_block = NULL;
// 			disk_inode->doubly_indirect_allocations = NULL;
// 			disk_inode->singly_indirect_block_sector = -1;
// 			disk_inode->doubly_indirect_block_sector = -1;
// 			disk_inode->doubly_indirect_allocations_sector = -1;
// 		}
//
// 		//If creating a file that only needs to use the inode's direct blocks
// 		else if(sectors >= 1 && sectors < NUM_INDIRECT_BLOCKS) {
// 			for(i = 0; i < sectors; i++) {
// 				free_map_allocate(1, &disk_inode->direct_blocks[i]);
// 				block_write(fs_device, disk_inode->direct_blocks[i], zeros);
// 				////////////printf("HELLLLOOOOO!!!!\t%u\n", disk_inode->direct_blocks[i]);
// 			}
// 			for(i = sectors; i < 124; i++) {
// 				disk_inode->direct_blocks[i] = -1;
// 				////////////printf("HELLLLOOOOO!!!!\t%u\n", disk_inode->direct_blocks[i]);
// 			}
// 			disk_inode->singly_indirect_block = NULL;
// 			disk_inode->doubly_indirect_block = NULL;
// 			disk_inode->singly_indirect_block_sector = -1;
// 			disk_inode->doubly_indirect_block_sector = -1;
// 		}
//
// 		//If creating a file that needs the inode's singly indirect block
// 		else if(sectors >= 125 && sectors <= 252) {
// 			//Allocate the inode's singly indirect block
// 			//TODO: Come back and make sure this is correct
// 			disk_inode->singly_indirect_block = calloc(1, BLOCK_SECTOR_SIZE);
// 			free_map_allocate(1, &disk_inode->singly_indirect_block_sector);
// 			//block_write(fs_device, disk_inode->singly_indirect_block, zeros);	shouldn't need this, write at the end of the else if block
//
// 			//Allocate the inode's direct blocks
// 			for(i = 0; i < 124; i++) {
// 				free_map_allocate(1, &disk_inode->direct_blocks[i]);
// 				block_write(fs_device, disk_inode->direct_blocks[i], zeros);
// 			}
// 			//Allocate the singly indirect block's direct blocks
// 			for(i = 0; i < (sectors - 124); i++) {
// 				free_map_allocate(1, &disk_inode->singly_indirect_block->direct_blocks[i]);
// 				block_write(fs_device, disk_inode->singly_indirect_block->direct_blocks[i], zeros);
// 			}
// 			for(i = (sectors - 124); i < 128; i++) {
// 				disk_inode->singly_indirect_block->direct_blocks[i] = -1;
// 			}
// 			//TODO: Need some way to update the on disk inode so the "-1"s show; may need to do an inode_write_at
// 			block_write(fs_device, sector, disk_inode);	//almost certainly wrong
// 			block_write(fs_device, disk_inode->singly_indirect_block_sector, disk_inode->singly_indirect_block);
// 			////free(disk_inode->singly_indirect_block);
//
// 			disk_inode->doubly_indirect_block = NULL;
// 			disk_inode->doubly_indirect_block_sector = -1;
// 		}
//
// 		//If creating a file that needs the inode's doubly indirect block
// 		else if(sectors >= 253 && sectors <= 16636) {
// 			// int indirect_blocks_needed = (sectors - 252) / 512;	//TODO: WRONG!!! need to round up, not truncate
// 			int indirect_blocks_needed = bytes_to_sectors(length - (512 * 252));	//TODO: still need to make sure this is right
// 			int direct_blocks_in_last_indirect_block = (sectors - 252) % 512;	//YAY for descriptive, crazy long variable names!
//
// 			//Allocate the inode's singly and doubly indirect blocks
// 			disk_inode->singly_indirect_block = calloc(1, BLOCK_SECTOR_SIZE);
// 			disk_inode->doubly_indirect_block = calloc(1, BLOCK_SECTOR_SIZE);
// 			free_map_allocate(1, &disk_inode->singly_indirect_block_sector);
// 			free_map_allocate(1, &disk_inode->doubly_indirect_block_sector);
//
// 			//Allocate the inode's direct blocks
// 			for(i = 0; i < 124; i++) {
// 				free_map_allocate(1, &disk_inode->direct_blocks[i]);
// 				block_write(fs_device, disk_inode->direct_blocks[i], zeros);
// 			}
//
// 			//Allocate the inode's direct blocks in the singly direct blocks
// 			for(i = 0; i < 128; i++) {
// 				free_map_allocate(1, &disk_inode->singly_indirect_block->direct_blocks[i]);
// 				block_write(fs_device, disk_inode->singly_indirect_block->direct_blocks[i], zeros);
// 			}
//
// 			//Allocate the blocks within the inode's doubly indirect block
// 			for(i = 0; i < indirect_blocks_needed - 2; i++) {
// 				//TODO: This is wrong now, need to allocate RAM and all that good stuff
// 				free_map_allocate(1, &disk_inode->doubly_indirect_block->singly_indirect_blocks[i]);
// 				block_write(fs_device, disk_inode->doubly_indirect_block->singly_indirect_blocks[i], zeros);
//
// 				for(j = 0; j < 128; j++) {
// 					free_map_allocate(1, &disk_inode->doubly_indirect_block->singly_indirect_blocks[i]->direct_blocks[j]);
// 					block_write(fs_device, disk_inode->doubly_indirect_block->singly_indirect_blocks[i]->direct_blocks[j], zeros);
// 				}
// 			}
//
// 			//Allocate the last block
// 			//TODO: What if this is the first block in an indirect block? Will it still work?
// 			free_map_allocate(1, &disk_inode->doubly_indirect_block->singly_indirect_blocks[indirect_blocks_needed -1]); //Off by 1 error maybe? I think this is right though
// 			block_write(fs_device, disk_inode->doubly_indirect_block->singly_indirect_blocks[indirect_blocks_needed - 1], zeros);
// 			for(i = 0; i < direct_blocks_in_last_indirect_block; i++) {
// 				free_map_allocate(1, &disk_inode->doubly_indirect_block->singly_indirect_blocks[indirect_blocks_needed - 1]->direct_blocks[i]);
// 				block_write(fs_device, disk_inode->doubly_indirect_block->singly_indirect_blocks[indirect_blocks_needed - 1]->direct_blocks[i], zeros);
// 			}
// 			for(i = direct_blocks_in_last_indirect_block; i < 128; i++) {
// 				disk_inode->doubly_indirect_block->singly_indirect_blocks[indirect_blocks_needed - 1]->direct_blocks[i] = -1;
// 			}
// 		}
// 		else {
// 			PANIC("Too many sectors in inode_create\n");
// 		}
// 		//Also need to write the inode itself I think, but that's a change from before and I need to reason through how to do it...
// 		block_write(fs_device, sector, disk_inode);
// 		////////////printf("About to free\n");
// 		// ////free(disk_inode);	//TODO: Won't free right now, try again later
// 		////////////printf("Finished freeing\n");
// 	}
//
// 	// return success;
// 	return true;
// }

//----------------------------SECOND TRY------------------------
/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
//TODO: need to update for new inode structure. Make sure to have the pointers point to NULL to keep file size down while you can
//TODO: also need to allow for a default value to be created
/*Much of this is similar to byte_to_sector, but it would be a waste to call
  byte_to_sector because no sectors have been allocated yet
	TODO: COULD pass another variable in to bytes_to_sector, like
	allocate_if_not_found...*/
	//Changed to use an inode* instead of a inode_disk* so we can use byte_to_sector
bool inode_create(block_sector_t sector, off_t length, bool is_dir) {
	////////////printf("Starting inode_create, creating with sector %d\n", sector);
	//////printf("Starting inode_create, using sector %d and length %d\n", sector, length);
	struct inode_disk* disk_inode;
	bool success = false;
	block_sector_t new_sector;	//Not strictly necessary, makes code easier to read though
	static char zeros[BLOCK_SECTOR_SIZE];
	int i, j;

	ASSERT(length >= 0);

	/* If this assertion fails, the inode structure is not exactly
	one sector in size, and you should fix that. */
	ASSERT(sizeof * disk_inode == BLOCK_SECTOR_SIZE);

	disk_inode = calloc(1, sizeof * disk_inode);
	if(disk_inode != NULL) {	//If allocation of memory was successful
		block_sector_t sector_idx = length / BLOCK_SECTOR_SIZE;
		int section = -1;

		disk_inode->length = length;
		disk_inode->magic = INODE_MAGIC;
		disk_inode->is_directory = is_dir ? true : false;
		disk_inode->parent_directory = -1;
		// for(i = 0; i < 10; i++)
		// 	////////printf("---------------------------------------\n");
		// ////////printf("inode->length is: %d\n", disk_inode->length);
		// for(i = 0; i < 10; i++)
			////////printf("---------------------------------------\n");

		/* Find how deep the inderection needs to go */
		if(length != 0) {
			if(sector_idx < FIRST_INDIRECT_BLOCK)
				section = 0;
			else if(sector_idx < FIRST_DOUBLY_INDIRECT_BLOCK)
				section = 1;
			else if(sector_idx < LAST_DOUBLY_INDIRECT_BLOCK + 1)
				section = 2;
			else
				PANIC("Assuming calculations are correct, trying to create a file larger than max file size\n");
		}

		/* If creating an empty file */
		if(length == 0) {
			////////printf("---File length 0, good!---\n");
			////////printf("Creating inode %d\n", sector);
			for(i = 0; i <= LAST_DIRECT_BLOCK; i++) {
				disk_inode->direct_blocks[i] = -1;
			}
			disk_inode->singly_indirect_block = NULL;
			disk_inode->doubly_indirect_block = NULL;
			disk_inode->doubly_indirect_allocations = NULL;
			disk_inode->singly_indirect_block_sector = -1;
			disk_inode->doubly_indirect_block_sector = -1;
			disk_inode->doubly_indirect_allocations_sector = -1;
		}

		switch(section) {
			/* Only using direct blocks */
			case 0: {
				/* Allocate the direct blocks needed */
				for(i = 0; i <= sector_idx; i++) {
					free_map_allocate(1, &new_sector);
					block_write(fs_device, new_sector, zeros);
					disk_inode->direct_blocks[i] = new_sector;
				}
				/* Set the direct blocks not needed to -1 */
				for(i = sector_idx + 1; i < FIRST_INDIRECT_BLOCK; i++) {
					disk_inode->direct_blocks[i] = -1;
				}

				/* Iniitallize other blocks to NULL */
				disk_inode->singly_indirect_block = NULL;
				disk_inode->doubly_indirect_block = NULL;
				disk_inode->doubly_indirect_allocations = NULL;
				disk_inode->singly_indirect_block_sector = -1;
				disk_inode->doubly_indirect_block_sector = -1;
				disk_inode->doubly_indirect_allocations_sector = -1;
				break;
			}

			/* Using the singly indirect block */
			case 1: {
				disk_inode->singly_indirect_block = calloc(1, BLOCK_SECTOR_SIZE);
				if(!disk_inode->singly_indirect_block) {
					PANIC("Damn...\n");
				}
				free_map_allocate(1, &new_sector);
				disk_inode->singly_indirect_block_sector = new_sector;

				/* Allocate the direct blocks */
				for(i = 0; i <= LAST_DIRECT_BLOCK; i++) {
					free_map_allocate(1, &new_sector);
					block_write(fs_device, new_sector, zeros);
					disk_inode->direct_blocks[i] = new_sector;
				}
				////printf("Finished allocating direct blocks\n");

				/* Allocate the singly indirect block */
				for(i = 0; i <= sector_idx - FIRST_INDIRECT_BLOCK; i++) {
					free_map_allocate(1, &new_sector);
					block_write(fs_device, new_sector, zeros);
					disk_inode->singly_indirect_block->direct_blocks[i] = new_sector;
				}
				////printf("Finished allocating needed blocks in indirect block\n");

				/* Set the blocks not needed to -1 */
				for(i = sector_idx - FIRST_INDIRECT_BLOCK + 1; i <= LAST_INDIRECT_BLOCK; i++) {
					disk_inode->singly_indirect_block->direct_blocks[i] = -1;
				}
				////printf("Initialized rest of indirect block to -1\n");

				/* Initiallize other blocks to NULL */
				disk_inode->doubly_indirect_block = NULL;
				disk_inode->doubly_indirect_allocations = NULL;
				disk_inode->doubly_indirect_block_sector = -1;
				disk_inode->doubly_indirect_allocations_sector = -1;

				block_write(fs_device, disk_inode->singly_indirect_block_sector, disk_inode->singly_indirect_block);
				////printf("Wrote the indirect block to disk\n");
				////printf("About to free the indirect block\n");
				//free(disk_inode->singly_indirect_block);
				////printf("Freed the indirect block\n");
				disk_inode->singly_indirect_block = NULL;

				////printf("Finishing case 1\n");

				break;
			}

			/* Using the doubly indirect block */
			//TODO: Sweet Jesus, make sure this is all correct
			case 2: {
				PANIC("Case 2 in inode_create\n");
				/* Allocate the pointers in the inode */
				disk_inode->singly_indirect_block = calloc(1, BLOCK_SECTOR_SIZE);
				disk_inode->doubly_indirect_block = calloc(1, BLOCK_SECTOR_SIZE);
				disk_inode->doubly_indirect_allocations = calloc(1, BLOCK_SECTOR_SIZE);
				free_map_allocate(1, &disk_inode->singly_indirect_block_sector);
				free_map_allocate(1, &disk_inode->doubly_indirect_block_sector);
				free_map_allocate(1, &disk_inode->doubly_indirect_allocations_sector);

				/* Allocate the direct blocks */
				for(i = 0; i<= LAST_DIRECT_BLOCK; i++) {
					free_map_allocate(1, &new_sector);
					block_write(fs_device, new_sector, zeros);
					disk_inode->direct_blocks[i] = new_sector;
				}

				/* Allocate the singly indirect block */
				for(i = 0; i < 128; i++) {
					free_map_allocate(1, &new_sector);
					block_write(fs_device, new_sector, zeros);
					disk_inode->singly_indirect_block->direct_blocks[i] = new_sector;
				}

				/* Allocate the doubly indirect block */
				//TODO: Make certain these are correct
				int indirect_index = (sector_idx - FIRST_DOUBLY_INDIRECT_BLOCK) / 128;
				int direct_index = (sector_idx - FIRST_DOUBLY_INDIRECT_BLOCK) % 128;

				//TODO: come back and check all for loops
				/* Allocate all but the last needed sector of the doubly indirect block */
				for(i = 0; i < indirect_index; i++) {
					/* Allocate the sector in the doubly indirect block */
					free_map_allocate(1, &disk_inode->doubly_indirect_allocations->indirect_block_sectors[i]);
					disk_inode->doubly_indirect_block->singly_indirect_blocks[i] = calloc(1, BLOCK_SECTOR_SIZE);

					/* Allocate the singly indirect block pointed to by the doubly indirect block */
					for(j = 0; j < 128; j++) {
						free_map_allocate(1, &new_sector);
						block_write(fs_device, new_sector, zeros);
						disk_inode->doubly_indirect_block->singly_indirect_blocks[i]->direct_blocks[j] = new_sector;
					}

					block_write(fs_device, disk_inode->doubly_indirect_allocations->indirect_block_sectors[i], disk_inode->doubly_indirect_block->singly_indirect_blocks[i]);
					//free(disk_inode->doubly_indirect_block->singly_indirect_blocks[i]);
					disk_inode->doubly_indirect_block->singly_indirect_blocks[i] = NULL;
				}

				/* Allocate the last needed sector of the doubly indirect block */
				free_map_allocate(1, &disk_inode->doubly_indirect_allocations->indirect_block_sectors[indirect_index]);
				disk_inode->doubly_indirect_block->singly_indirect_blocks[indirect_index] = calloc(1, BLOCK_SECTOR_SIZE);
				for(i = 0; i <= direct_index; i++) {
					free_map_allocate(1, &new_sector);
					block_write(fs_device, new_sector, zeros);
					disk_inode->doubly_indirect_block->singly_indirect_blocks[indirect_index]->direct_blocks[i] = new_sector;
				}
				/* Set the remainder of the last block to -1 */
				for(i = direct_index + 1; i < 128; i++) {
					disk_inode->doubly_indirect_block->singly_indirect_blocks[indirect_index]->direct_blocks[i] = -1;
				}
				block_write(fs_device, disk_inode->doubly_indirect_allocations->indirect_block_sectors[indirect_index], disk_inode->doubly_indirect_block->singly_indirect_blocks[indirect_index]);
				//free(disk_inode->doubly_indirect_block->singly_indirect_blocks[indirect_index]);
				disk_inode->doubly_indirect_block->singly_indirect_blocks[indirect_index] = NULL;

				/* Write the remaining blocks on the inode to disk */
				block_write(fs_device, disk_inode->singly_indirect_block_sector, disk_inode->singly_indirect_block);
				block_write(fs_device, disk_inode->doubly_indirect_block_sector, disk_inode->doubly_indirect_block);
				block_write(fs_device, disk_inode->doubly_indirect_allocations_sector, disk_inode->doubly_indirect_allocations);
				//free(disk_inode->singly_indirect_block);
				//free(disk_inode->doubly_indirect_block);
				//free(disk_inode->doubly_indirect_allocations);
				disk_inode->singly_indirect_block = NULL;
				disk_inode->doubly_indirect_block = NULL;
				disk_inode->doubly_indirect_allocations = NULL;

				break;
			}
			default:
				break;
		}

		block_write(fs_device, sector, disk_inode);
		success = true;
		//////printf("At end of create for sector %d, length is %d\n", sector, disk_inode->length);
	}

	free(disk_inode);
	disk_inode = NULL;

	return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
//TODO: I think it's good now, come back to check though
struct inode* inode_open (block_sector_t sector) {
	////////////printf("Starting inode_open, opening sector %d\n", sector);
	struct list_elem* e;
	struct inode* inode;
	// //////printf("Opening from sector %d\n", sector);

	/* Check whether this inode is already open. */
	for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
	     e = list_next (e)) {
		inode = list_entry (e, struct inode, elem);
		if (inode->sector == sector) {
			inode_reopen (inode);
			return inode;
		}
	}

	/* Allocate memory if inode is not already open. */
	inode = malloc (sizeof * inode);
	if (inode == NULL) {
		return NULL;
	}

	/* Initialize. */
	list_push_front (&open_inodes, &inode->elem);
	inode->sector = sector;
	inode->open_cnt = 1;
	inode->deny_write_cnt = 0;
	inode->removed = false;
	block_read (fs_device, inode->sector, &inode->data);

	//////printf("Opening from sector %d, length of inode is %d\n", sector, inode->data.length);

	/* Allocate memory for the indirect blocks */
	/* Singly indirect block */
	if(inode->data.singly_indirect_block_sector != -1) {
		inode->data.singly_indirect_block = calloc(1, BLOCK_SECTOR_SIZE);
		block_read(fs_device, inode->data.singly_indirect_block_sector, inode->data.singly_indirect_block);
	}
	/* Doubly indirect block */
	if(inode->data.doubly_indirect_block_sector != -1) {
		if(inode->data.doubly_indirect_allocations_sector == -1)
			PANIC("Doubly indirect block has a sector number, but the allocations sector does not. This should NEVER happen\n");

		int i;

		inode->data.doubly_indirect_block = calloc(1, BLOCK_SECTOR_SIZE);
		inode->data.doubly_indirect_allocations = calloc(1, BLOCK_SECTOR_SIZE);
		block_read(fs_device, inode->data.doubly_indirect_block_sector, inode->data.doubly_indirect_block);
		block_read(fs_device, inode->data.doubly_indirect_allocations_sector, inode->data.doubly_indirect_allocations);

		/* Allocate the singly indirect blocks in the doubly indirect block */
		for(i = 0; i < 128; i++) {
			if(inode->data.doubly_indirect_allocations->indirect_block_sectors[i] != -1)
				inode->data.doubly_indirect_block->singly_indirect_blocks[i] = calloc(1, BLOCK_SECTOR_SIZE);
				block_read(fs_device, inode->data.doubly_indirect_allocations->indirect_block_sectors[i], inode->data.doubly_indirect_block->singly_indirect_blocks[i]);
		}
	}

	return inode;
}

/* Reopens and returns INODE. */
struct inode*
inode_reopen (struct inode* inode) {
	if (inode != NULL) {
		inode->open_cnt++;
	}

	return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode* inode) {
	return inode->sector;
}

//----------------------------ORIGINAL------------------------
/* Closes INODE and writes it to disk. (Does it?  Check code.)
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
//TODO: update to make sure to free allocated pointers if necessary
//The free map release is also wrong now
// void
// inode_close (struct inode* inode) {
// 	////////////printf("In inode_close\n");
// 	/* Ignore null pointer. */
// 	if (inode == NULL) {
// 		return;
// 	}
//
// 	/* Release resources if this was the last opener. */
// 	if (--inode->open_cnt == 0) {
// 		/* Remove from inode list and release lock. */
// 		list_remove (&inode->elem);
//
// 		/* Deallocate blocks if removed. */
// 		if (inode->removed) {
// 			free_map_release (inode->sector, 1);
// 			free_map_release (inode->data.start,
// 			                  bytes_to_sectors (inode->data.length));
// 		}
//
// 		free (inode);
// 	}
// }

// //----------------------------FIRST TRY------------------------
// /* Closes INODE and writes it to disk. (Does it?  Check code.)
//    If this was the last reference to INODE, frees its memory.
//    If INODE was also a removed inode, frees its blocks. */
// /* Doesn't zero everything out if removed, only frees the RAM and sectors */
// void inode_close(struct inode* inode) {
// 	////////////printf("In inode_close\n");
// 	int i, j;
//
// 	/* Ignore null pointer */
// 	if(inode == NULL)
// 		return;
//
// 	/* Release resources if this was the last opener. */
// 	if(--inode->open_cnt == 0) {
// 		/* Remove from inode list and release lock. */
// 		list_remove(&inode->elem);
//
// 		/* Deallocate blocks if removed. */
// 		if(inode->removed) {
// 			/* Deallocate direct blocks */
// 			for(i = 0; i < LAST_DIRECT_BLOCK; i++) {
// 				if(inode->data.direct_blocks[i] != -1) {
// 					free_map_release(inode->data.direct_blocks[i], 1);
// 				}
// 			}
// 			/* Deallocate the singly indirect block */
// 			if(inode->data.singly_indirect_block) {
// 				for(i = 0; i < 128; i++) {
// 					if(inode->data.singly_indirect_block->direct_blocks[i] != -1) {
// 						free_map_release(inode->data.singly_indirect_block->direct_blocks[i], 1);
// 					}
// 				}
// 				free_map_release(inode->data.singly_indirect_block_sector, 1);
// 			}
// 			/* Deallocate the doubly indirect block */
// 			if(inode->data.doubly_indirect_block) {
// 				for(i = 0; i < 128; i++) {
// 					if(inode->data.doubly_indirect_block->singly_indirect_blocks[i]) {
// 						for(j = 0; j < 128; j++) {
// 							if(inode->data.doubly_indirect_block->singly_indirect_blocks[i]->direct_blocks[j] != -1) {
// 								free_map_release(inode->data.doubly_indirect_block->singly_indirect_blocks[i]->direct_blocks[j], 1);
// 							}
// 						}
// 						free_map_release(inode->data.doubly_indirect_block->singly_indirect_blocks[i], 1);
// 					}
// 				}
// 				free_map_release(inode->data.doubly_indirect_block_sector, 1);
// 			}
// 		}
//
// 		/* Free the RAM */
// 		if(inode->data.singly_indirect_block)
// 			////free(inode->data.singly_indirect_block);
// 		if(inode->data.doubly_indirect_block)
// 			////free(inode->data.doubly_indirect_block);
// 	}
// }

//----------------------------SECOND TRY------------------------
/* Closes INODE and writes it to disk. (Does it?  Check code.)
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void inode_close(struct inode* inode) {
	////////printf("INODE IS BEING CLOSED!!!!!!\n");
	int i, j;

	/* Ignore null pointer */
	if(inode == NULL) {
		return;
	}

	/* Release resources if this was the last opener */
	if(--inode->open_cnt == 0) {
		/* Remove from inode list and release lock */
		list_remove(&inode->elem);

		/* If inode->removed, free sectors and RAM */
		if(inode->removed) {
			/* Direct blocks */
			for(i = 0; i <= LAST_DIRECT_BLOCK; i++) {
				if(inode->data.direct_blocks[i] != -1)
					free_map_release(inode->data.direct_blocks[i], 1);
			}

			/* Singly indirect block */
			if(inode->data.singly_indirect_block_sector != -1) {
				for(i = 0; i < 128; i++) {
					if(inode->data.singly_indirect_block->direct_blocks[i] != -1)
						free_map_release(inode->data.singly_indirect_block->direct_blocks[i], 1);
				}

				free_map_release(inode->data.singly_indirect_block_sector, 1);
				//free(inode->data.singly_indirect_block);
				inode->data.singly_indirect_block = NULL;
			}

			/* Doubly indirect block */
			if(inode->data.doubly_indirect_block_sector != -1) {
				for(i = 0; i < 128; i++) {
					/* If there is a singly indirect block allocated at the index */
					if(inode->data.doubly_indirect_allocations->indirect_block_sectors[i] != -1) {
						for(j = 0; j < 128; j++) {
							if(inode->data.doubly_indirect_block->singly_indirect_blocks[i]->direct_blocks[j] != -1)
								free_map_release(inode->data.doubly_indirect_block->singly_indirect_blocks[i]->direct_blocks[j], 1);
						}

						free_map_release(inode->data.doubly_indirect_allocations->indirect_block_sectors[i], 1);
						//free(inode->data.doubly_indirect_block->singly_indirect_blocks[i]);
						inode->data.doubly_indirect_block->singly_indirect_blocks[i] = NULL;
					}
				}
				free_map_release(inode->data.doubly_indirect_block_sector, 1);
				free_map_release(inode->data.doubly_indirect_allocations_sector, 1);
				//free(inode->data.doubly_indirect_block);
				//free(inode->data.doubly_indirect_allocations);
				inode->data.doubly_indirect_block = NULL;
				inode->data.doubly_indirect_allocations = NULL;
			}
		}

		/* If not removed, only free RAM */
		else {
			/* Singly indirect block */
			if(inode->data.singly_indirect_block) {
				//free(inode->data.singly_indirect_block);
				inode->data.singly_indirect_block = NULL;
			}

			/* Doubly indirect block */
			if(inode->data.doubly_indirect_block) {
				for(i = 0; i < 128; i++) {
					if(inode->data.doubly_indirect_block->singly_indirect_blocks[i]) {
						//free(inode->data.doubly_indirect_block->singly_indirect_blocks[i]);
						inode->data.doubly_indirect_block->singly_indirect_blocks[i] = NULL;
					}
				}
				if(!inode->data.doubly_indirect_allocations)
					PANIC("Freeing doubly indirect block, but no doubly indirect allocations, somewhere something went wrong\n");
				//free(inode->data.doubly_indirect_block);
				//free(inode->data.doubly_indirect_allocations);
				inode->data.doubly_indirect_block = NULL;
				inode->data.doubly_indirect_allocations = NULL;
			}
		}



		// /* Free RAM (and deallocate sectors if removed) */
		// /* Direct blocks */
		// if(inode->removed) {
		// 	for(i = 0; i < NUM_DIRECT_BLOCKS; i++) {
		// 		if(inode->data.direct_blocks[i] != -1)
		// 			free_map_release(inode->data.direct_blocks[i], 1);
		// 	}
		// }
		//
		// /* Singly indirect block */
		// if(inode->data.singly_indirect_block_sector != -1) {
		// 	if(inode->removed) {
		// 		for(i = 0; i < 128; i++) {
		// 			if(inode->data.singly_indirect_block->direct_blocks[i] != -1) {
		// 				free_map_release(inode->data.singly_indirect_block->direct_blocks[i], 1);
		// 			}
		// 		}
		// 		free_map_release(inode->data.singly_indirect_block_sector, 1);
		// 	}
		// 	////free(inode->data.singly_indirect_block);
		// }
		//
		// /* Doubly indirect block */
		// if(inode->data.doubly_indirect_block_sector != -1) {
		// 	for(i = 0; i < 128; i++) {
		// 		if(inode->data.doubly_indirect_allocations->indirect_block_sectors[i] != -1) {
		// 			if(inode->removed) {
		// 				for(j = 0; j < 128; j++) {
		// 					if(inode->data.doubly_indirect_block->singly_indirect_blocks[i]->direct_blocks[j] != -1) {
		// 						free_map_release(inode->data.doubly_indirect_block->singly_indirect_blocks[i]->direct_blocks[j], 1);
		// 					}
		// 				}
		// 				free_map_release(inode->data.doubly_indirect_allocations->indirect_block_sectors[i], 1);
		// 			}
		// 			////free(inode->data.doubly_indirect_block->singly_indirect_blocks[i]);
		// 		}
		// 	}
		//
		// 	if(inode->removed) {
		// 		free_map_release(inode->data.doubly_indirect_block_sector, 1);
		// 		free_map_release(inode->data.doubly_indirect_allocations_sector, 1);
		// 	}

			////free(inode->data.doubly_indirect_block);
			////free(inode->data.doubly_indirect_allocations);
		// }

		// /* Deallocate blocks if removed */
		// if(inode->removed) {
		// 	int i, j;
		//
		// 	/* Deallocate the direct blocks */
		// 	for(i = 0; i < NUM_DIRECT_BLOCKS; i++) {
		// 		if(inode->data.direct_blocks[i] != -1)
		// 			free_map_release(inode->data.direct_blocks[i], 1);
		// 	}
		//
		// 	/* Deallocate the singly indirect block */
		// 	if(inode->data.singly_indirect_block_sector != -1) {
		// 		for(i = 0; i < 128; i++) {
		// 			if(inode->data.singly_indirect_block->direct_blocks[i] != -1)
		// 				free_map_release(inode->data.singly_indirect_block->direct_blocks[i], 1);
		// 		}
		//
		// 		////free(inode->data.singly_indirect_block);
		// 		free_map_release(inode->data.singly_indirect_block_sector, 1);
		// 	}
		//
		// 	/* Deallocate the doubly indirect block */
		// 	if(inode->data.doubly_indirect_block_sector != -1) {
		// 		for(i = 0; i < 128; i++) {
		// 			if(inode->data.doubly_indirect_allocations->indirect_block_sectors[i] != -1) {
		// 				for(j = 0; j < 128; j++) {
		// 					if(inode->data.doubly_indirect_block->singly_indirect_blocks[i]->direct_blocks[j] != -1)
		// 					free_map_release(inode->data.doubly_indirect_block->singly_indirect_blocks[i]->direct_blocks[j], 1);
		// 				}
		//
		// 				////free(inode->data.doubly_indirect_block->singly_indirect_blocks[i]);
		// 				free_map_release(inode->data.doubly_indirect_allocations->indirect_block_sectors[i], 1);
		// 			}
		// 		}
		//
		// 		////free(inode->data.doubly_indirect_block);
		// 		////free(inode->data.doubly_indirect_allocations);
		// 		free_map_release(inode->data.doubly_indirect_block_sector, 1);
		// 		free_map_release(inode->data.doubly_indirect_allocations_sector, 1);
		// 	}
		//
		// 	/* Deallocate the on disk inode itself */
		// 	free_map_release(inode->sector, 1);
		// }

		////free(inode);
		////////////printf("Finished inode_get_inumber\n");
	}
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode* inode) {
	////////////printf("Starting inode_remove, removing inode %d\n", inode->sector);
	ASSERT (inode != NULL);
	inode->removed = true;
	////////////printf("Finished inode_remove\n");
}

//----------------------------ORIGINAL------------------------
/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
//TODO: need to change to check ranges (if reading from direct, singly indirect or doubly indirect sectors)
// off_t
// inode_read_at (struct inode* inode, void* buffer_, off_t size, off_t offset) {
// 	uint8_t* buffer = buffer_;		//buffer that holds 1 byte at a time
// 	off_t bytes_read = 0;
// 	uint8_t* bounce = NULL;
//
// 	while (size > 0) {
// 		/* Disk sector to read, starting byte offset within sector. */
// 		block_sector_t sector_idx = byte_to_sector (inode, offset);
// 		int sector_ofs = offset % BLOCK_SECTOR_SIZE;
//
// 		/* Bytes left in inode, bytes left in sector, lesser of the two. */
// 		off_t inode_left = inode_length (inode) - offset;
// 		int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
// 		int min_left = inode_left < sector_left ? inode_left : sector_left;
//
// 		/* Number of bytes to actually copy out of this sector. */
// 		int chunk_size = size < min_left ? size : min_left;
// 		if (chunk_size <= 0) {
// 			break;
// 		}
//
// 		if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE) {
// 			/* Read full sector directly into caller's buffer. */
// 			block_read (fs_device, sector_idx, buffer + bytes_read);
// 		} else {
// 			/* Read sector into bounce buffer, then partially copy
// 			   into caller's buffer. */
// 			if (bounce == NULL) {
// 				bounce = malloc (BLOCK_SECTOR_SIZE);
// 				if (bounce == NULL) {
// 					break;
// 				}
// 			}
// 			block_read (fs_device, sector_idx, bounce);
// 			memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
// 		}
//
// 		/* Advance. */
// 		size -= chunk_size;
// 		offset += chunk_size;
// 		bytes_read += chunk_size;
// 	}
// 	free (bounce);
//
// 	return bytes_read;
// }

//----------------------------FIRST TRY------------------------
// /* TODO: could be optimized greatly by not traversing the structure after each sector */
// off_t inode_read_at(struct inode* inode, void* buffer_, off_t size, off_t offset) {
// 	////////////printf("In inode_read_at\n");
// 	uint8_t* buffer = buffer_;
// 	off_t bytes_read = 0;
// 	uint8_t* bounce = NULL;
//
// 	int section;
// 	uint32_t* sector_idx;
// 	int sector_ofs;
//
// 	if(offset < 0) {
// 		PANIC("You dun goofed, reading from a negative address relative to the file\n");
// 	}
// 	//TODO: Should this simply read as much of the file as possible and then return instead of panicking?
// 	if(offset > 8517631) {
// 		PANIC("You dun goofed, reading outside of the max file size (~8MB)\n");
// 	}
// 	if(offset < 63488) {
// 		section = 0;
// 	}
// 	else if(offset < 129024) {
// 		section = 1;
// 	}
// 	else {
// 		section = 2;
// 	}
// 	//TODO: should I check for out of bounds? I don't think so...
//
// 	while(size > 0) {
// 		//need brackets on each case for some reason, compile error without it (has to do with declaring a variable as the first statement in a case)
// 		switch(section) {
// 			/* Accessing direct blocks */
// 			case 0: {
// 				int direct_index = offset / BLOCK_SECTOR_SIZE;	//TODO: byte_to_sector() perhaps?
// 				//should this be a block_sector_t and use byte_to_sector?
// 				sector_idx = inode->data.direct_blocks[direct_index];	//pointer to location of the sector
// 				break;
// 			}
// 			/* Accessing singly indirect blocks */
// 			case 1: {
// 				int direct_index = (offset - 63488) / BLOCK_SECTOR_SIZE;
// 				sector_idx = inode->data.singly_indirect_block->direct_blocks[direct_index];
// 				break;
// 			}
// 			/* Accessing doubly indirect blocks */
// 			case 2: {
// 				int indirect_index = (offset - 129024) / (BLOCK_SECTOR_SIZE * 128);
// 				int direct_index = indirect_index / BLOCK_SECTOR_SIZE;
// 				sector_idx = inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index]->direct_blocks[direct_index];
// 				break;
// 			}
// 			/* Something went wrong */
// 			default: {
// 				PANIC("\"case\" set to something other than 0, 1 or 2\n");
// 			}
// 		}
//
// 		if(sector_idx == NULL) {
// 			PANIC("Accessing uninitialized sector (should probably just return here)\n");
// 		}
//
// 		sector_ofs = offset % BLOCK_SECTOR_SIZE;
//
// 		off_t sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
//
// 		int chunk_size;
// 		if(size < sector_left) {
// 			chunk_size = size;
// 		}
// 		else {
// 			chunk_size = sector_left;
// 		}
//
// 		if(chunk_size <= 0) {
// 			break;
// 		}
//
// 		if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE) {
// 			/* Read full sector directly into caller's buffer. */
// 			block_read (fs_device, sector_idx, buffer + bytes_read);
// 		} else {
// 			/* Read sector into bounce buffer, then partially copy
// 			   into caller's buffer. */
// 			if (bounce == NULL) {
// 				bounce = malloc (BLOCK_SECTOR_SIZE);
// 				if (bounce == NULL) {
// 					break;
// 				}
// 			}
// 			block_read (fs_device, sector_idx, bounce);	//read the sector into the bounce buffer
// 			memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size); //read chunk_size bytes from bounce to the buffer
// 		}
//
// 		/* Advance. */
// 		size -= chunk_size;
// 		offset += chunk_size;
// 		bytes_read += chunk_size;
//
// 		//Check if you are in a new section now
// 		//TODO: do I need to check for EOF or out of bounds? What happens if they request more than the EOF?
// 		//seems like the PDF said allow past EOF, still not sure about past max size (vulnerable to attacks)
// 		if(offset < 0) {
// 			PANIC("Should this happen?\n");
// 		}
// 		if(offset > 8517631) {
// 			PANIC("Should probably just terminate the call at this point (reading past the max file size)\n");
// 		}
// 		if(offset >= 63488 && offset < 129024) {
// 			section = 1;
// 		}
// 		else if(offset >= 129024 && offset <= 8517631) {
// 			section = 2;
// 		}
// 	}
// 	////free(bounce);
//
// 	return bytes_read;
// }

//----------------------------SECOND TRY------------------------
/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
//TODO: check if the requested data is within the inode's length
// off_t inode_read_at(struct inode* inode, void* buffer_, off_t size, off_t offset) {
// 	////////////printf("Starting inode_read_at\n");
// 	PANIC("Please tell me we don't use this...\n");
//
// 	uint8_t* buffer = buffer_;
// 	off_t bytes_read = 0;
// 	uint8_t* bounce = malloc(BLOCK_SECTOR_SIZE);
// 	if(!bounce)
// 		PANIC("Not enough memory to allocate bounce buffer on read\n");
//
// 	while(size > 0) {
// 		int section;
// 		int sector_idx = bytes_to_sectors(offset);	//Index sector would be in if the file was seen as being contiguous
// 		int sector_ofs = offset % BLOCK_SECTOR_SIZE;
// 		block_sector_t sector_to_read;
//
// 		if(sector_idx < LAST_DIRECT_BLOCK)
// 			section = 0;
// 		else if(sector_idx < LAST_INDIRECT_BLOCK)
// 			section = 1;
// 		else if(sector_idx < LAST_DOUBLY_INDIRECT_BLOCK)
// 			section = 2;
// 		else
// 			PANIC("Trying to read outside of the max file size\n");
//
// 		/* Used to find which sector to actually read */
// 		switch(section) {
// 			case 0: {
// 				sector_to_read = inode->data.direct_blocks[sector_idx];
// 				break;
// 			}
// 			case 1: {
// 				if(!inode->data.singly_indirect_block) //TODO: should return instead of panicking
// 					PANIC("Trying to read from singly indirect block, but it is not allocated\n");
//
// 				int direct_index = sector_idx - FIRST_INDIRECT_BLOCK;
// 				sector_to_read = inode->data.singly_indirect_block->direct_blocks[direct_index];
// 				if(sector_to_read == -1)
// 					PANIC("Trying to read from a block in the singly indirect block, but it is not allocated yet)\n");	//TODO: should return instead of panicking
// 					//TODO: Do something, this block has not been allocated yet
// 				break;
// 			}
// 			case 2: {
// 				int indirect_index = (sector_idx - FIRST_DOUBLY_INDIRECT_BLOCK) / 128;
// 				int direct_index = (sector_idx - FIRST_DOUBLY_INDIRECT_BLOCK) % 128;
//
// 				if(!inode->data.doubly_indirect_block) //TODO: should return instead of panicking
// 					PANIC("Trying to read from doubly indirect block, but it is not allocated\n");
// 				if(!inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index])
// 					PANIC("Singly indirect block in doubly indirect block not allocated\n");
// 				if(inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index]->direct_blocks[direct_index] == -1)
// 					PANIC("Direct block in a doubly indirect block not allocated\n");
//
// 				sector_to_read = inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index]->direct_blocks[direct_index];
// 				break;
// 			default:
// 				PANIC("In switch in inode_read, somehow section not 0 1 or 2\n");
// 				break;
// 			}
// 		}
//
// 		/* Size to actually read from the sector */
// 		int chunk_size = size < BLOCK_SECTOR_SIZE ? size : BLOCK_SECTOR_SIZE;
//
// 		/* If reading an entire sector */
// 		if(sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE) {
// 			block_read(fs_device, sector_to_read, buffer + bytes_read);
// 		}
// 		else {
// 			block_read(fs_device, sector_to_read, bounce);
// 			memcpy(buffer + bytes_read, bounce + sector_ofs, chunk_size);
// 		}
//
// 		size -= chunk_size;
// 		offset += chunk_size;
// 		bytes_read += chunk_size;
// 		//TODO: still need a bit more here, like with the sector_idx I think
// 		sector_idx++;	//TODO: I think... Reading 1 sector at a time, so it should always move on to the next sector?
// 	}
// 	////free(bounce);
//
// 	////////////printf("Finished inode_read_at, returning %d\n", bytes_read);
// 	return bytes_read;
// }

//----------------------------THIRD TRY------------------------
/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
//TODO: check if the requested data is within the inode's length
off_t inode_read_at(struct inode* inode, void* buffer_, off_t size, off_t offset) {
	////////////printf("Starting inode_read_at, reading inode %d\n", inode->sector);
	////////printf("In inode_read_at\n");
	uint8_t* buffer = buffer_;
	off_t bytes_read = 0;
	uint8_t* bounce = NULL;

	if(inode == NULL) {
		////////printf("inode is NULL....\n");
	}
	////////printf("inode: %p\n", inode);
	////////printf("inode->open_cnt: %d\n", inode->open_cnt);

	////////printf("In inode_read_at, inode number is: %d\n", inode->sector);
	////////printf("inode->data.length is: %d\n", inode->data.length);
	////////printf("Location to read from is %d\n", offset);

	while(size > 0) {
		//TODO: If past EOF, return
		int count = 1;
		////////printf("On loop %d\n", count++);
		/* Disk sector to read, starting byte offset within sector */
		////////printf("Immediately before byte_to_sector\n");
		////////printf("Here, inode->data.length = %d\n", inode->data.length);
		////////printf("inode address immediately before byte_to_sector: %p\n", inode);
		////////printf("inode->sector immediately before byte_to_sector: %d\n", inode->sector);
		//////printf("Calling byte_to_sector in read, offset is %d\n", offset);
		//printf("finding sector to read, offset is %d\n", offset);
		//printf("inode->sector is %d\n", inode->sector);
		//printf("inode->data.direct_blocks[0] is %d\n", inode->data.direct_blocks[0]);
		block_sector_t sector_to_read = byte_to_sector(inode, offset, false);
		////////printf("inode address immediately after byte_to_sector: %p\n", inode);
		////////printf("inode->sector immediately after byte_to_sector: %d\n", inode->sector);
		int sector_ofs = offset % BLOCK_SECTOR_SIZE;
		////////printf("After byte_to_sector\n");

		/* Bytes left in inode, bytes left in sector, lesser of the two
		   This still works because the inode is seen as being contiguous, and a
			 read does not extend the file */
		off_t inode_left = inode->data.length - offset;
		int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually copy out of this sector */
		int chunk_size = size < min_left ? size : min_left;
		////////printf("Before break\n");
		if(chunk_size <= 0)
			break;
		////////printf("After break\n");

		if(sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE) {
			/* Read full sector directly into caller's buffer */
			//printf("Sector_to_read is %d\n", sector_to_read);
			block_read(fs_device, sector_to_read, buffer + bytes_read);
			////////printf("In if\n");
		}
		else {
			////////printf("In else\n");
			/* Read sector into bounce buffer, then partially copy
			   into caller's buffer */
			if(bounce == NULL) {
				////////printf("Bounce is null\n");
				bounce = malloc(BLOCK_SECTOR_SIZE);
				if(bounce == NULL) {
					////////printf("Bounce is null again!\n");
					break;
				}
			}
			////////printf("Before block read\n");
			block_read(fs_device, sector_to_read, bounce);
			////////printf("After block_read, before memcpy\n");
			////////printf("One more ////////printf for good measure\n");
			memcpy(buffer + bytes_read, bounce + sector_ofs, chunk_size);
			////////printf("After memcpy\n");
		}

		////////////printf("Sector read: %d\n", sector_to_read);

		/* Advance */
		//////printf("Previous offset: %d\n", offset);
		size -= chunk_size;
		offset += chunk_size;
		bytes_read += chunk_size;
		//////printf("New offset: %d\n", offset);
	}
	if(bounce) {
		////////printf("Bounce's address: %p\n", bounce);
		//free(bounce);
		bounce = NULL;
	}

	////////printf("inode->sector immediately before end of inode_read_at: %d\n", inode->sector);
	////////printf("inode pointer immediately before end of inode_read_at: %p\n", inode);

	////////printf("Finished inode_read_at, bytes read: %d\n", bytes_read);
	return bytes_read;
}

//-------------------ORIGINAL---------------------------
// /* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
//    Returns the number of bytes actually written, which may be
//    less than SIZE if end of file is reached or an error occurs.
//    (Normally a write at end of file would extend the inode, but
//    growth is not yet implemented.) */
// off_t
// inode_write_at (struct inode *inode, const void *buffer_, off_t size,
//                 off_t offset)
// {
//   const uint8_t *buffer = buffer_;
//   off_t bytes_written = 0;
//   uint8_t *bounce = NULL;
//
//   if (inode->deny_write_cnt)
//     return 0;
//
//   while (size > 0)
//     {
//       /* Sector to write, starting byte offset within sector. */
//       block_sector_t sector_idx = byte_to_sector (inode, offset);
//       int sector_ofs = offset % BLOCK_SECTOR_SIZE;
//
//       /* Bytes left in inode, bytes left in sector, lesser of the two. */
//       off_t inode_left = inode_length (inode) - offset;
//       int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
//       int min_left = inode_left < sector_left ? inode_left : sector_left;
//
//       /* Number of bytes to actually write into this sector. */
//       int chunk_size = size < min_left ? size : min_left;
//       if (chunk_size <= 0)
//         break;
//
//       if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
//         {
//           /* Write full sector directly to disk. */
//           block_write (fs_device, sector_idx, buffer + bytes_written);
//         }
//       else
//         {
//           /* We need a bounce buffer. */
//           if (bounce == NULL)
//             {
//               bounce = malloc (BLOCK_SECTOR_SIZE);
//               if (bounce == NULL)
//                 break;
//             }
//
//           /* If the sector contains data before or after the chunk
//              we're writing, then we need to read in the sector
//              first.  Otherwise we start with a sector of all zeros. */
//           if (sector_ofs > 0 || chunk_size < sector_left)
//             block_read (fs_device, sector_idx, bounce);
//           else
//             memset (bounce, 0, BLOCK_SECTOR_SIZE);
//           memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
//           block_write (fs_device, sector_idx, bounce);
//         }
//
//       /* Advance. */
//       size -= chunk_size;
//       offset += chunk_size;
//       bytes_written += chunk_size;
//     }
//   free (bounce);
//
//   return bytes_written;
// }

//-------------------FIRST TRY---------------------------
/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
//TODO: update to grow file when you run out of the current type of blocks (direct, singly indirect, doubly indirect)
// off_t
// inode_write_at (struct inode* inode, const void* buffer_, off_t size,
//                 off_t offset) {
// 	////////////printf("In inode_write_at\n");
// 	const uint8_t* buffer = buffer_;
// 	off_t bytes_written = 0;
// 	uint8_t* bounce = NULL;
//
// 	int section;
// 	// uint32_t* sector_idx;
// 	block_sector_t sector_idx;
// 	int sector_ofs;
// 	// block_sector_t* new_sector = NULL; //Used to hold a newly allocated sector
// 	block_sector_t new_sector;	//may not actually need
// 	static char zeros[BLOCK_SECTOR_SIZE];	//Used to zero out new sectors
//
// 	if (inode->deny_write_cnt) {
// 		return 0;
// 	}
//
// 	if(offset < 0) {
// 		PANIC("You dun goofed, writing to a negative address relative to the file\n");
// 	}
// 	//TODO: should probably just have this return at this point
// 	if(offset > 8517631) {
// 		PANIC("You dun goofed, writing outside of the max file size (~8MB)\n");
// 	}
// 	if(offset < 63488) {
// 		section = 0;
// 	}
// 	else if(offset < 129024) {
// 		section = 1;
// 	}
// 	else {
// 		section = 2;
// 	}
//
// 	while (size > 0) {
// 		/* Find sector index to write to, allocate sector if needed */
// 		switch(section) {
// 			case 0: {
// 				////////////printf("Got to case 0\n");
// 				int direct_index = offset / BLOCK_SECTOR_SIZE;
//
// 				/* If the sector to write to does not exist, allocate it */
// 				if(inode->data.direct_blocks[direct_index] == -1) {
// 					////////////printf("About to allocate a new sector\n");
// 					//allocate a new sector
// 					if(!free_map_allocate(1, &new_sector)) {
// 						////////////printf("Not panicking...\n");
// 						PANIC("Couln't allocate a new sector for case 0 for a direct block (should only happen if disk is full (I think))\n");
// 					}
// 					////////////printf("Allocated a new sector\n");
// 					////////////printf("Before block_write 1\n");
// 					block_write(fs_device, new_sector, zeros);
// 					////////////printf("After block_write 1\n");
// 					inode->data.direct_blocks[direct_index] = &new_sector;
// 				}
//
// 				sector_idx = inode->data.direct_blocks[direct_index];
// 				break;
// 			}
// 			case 1: {
// 				////////////printf("Got to case 1\n");
// 				int direct_index = (offset - 63488) / BLOCK_SECTOR_SIZE;
//
// 				/* If the singly indirect block has not been instantiated yet, allocate it */
// 				if(inode->data.singly_indirect_block == NULL) {
// 					if(!free_map_allocate(1, &new_sector)) {
// 						PANIC("Couldn't allocate a new sector for case 1 for the singly_indirect_block\n");
// 					}
//
// 					block_write(fs_device, new_sector, zeros);
// 					inode->data.singly_indirect_block = &new_sector;	//throws warning about types, but ____
// 				}
//
// 				/* If the sector to write to did not exist, allocate it */
// 				if(inode->data.singly_indirect_block->direct_blocks[direct_index] == NULL) {
// 					if(!free_map_allocate(1, &new_sector)) {
// 						PANIC("Couldn't allocate a new sector for case 1 for the direct_blocks in the singly_indirect_block\n");
// 					}
//
// 					block_write(fs_device, new_sector, zeros);
// 					inode->data.singly_indirect_block->direct_blocks[direct_index] = &new_sector;
// 				}
//
// 				sector_idx = inode->data.singly_indirect_block->direct_blocks[direct_index];
// 				break;
// 			}
// 			case 2: {
// 				////////////printf("Got to case 2\n");
// 				int indirect_index = (offset - 129024) / (BLOCK_SECTOR_SIZE * 128);
// 				int direct_index = indirect_index / BLOCK_SECTOR_SIZE;
//
// 				/* If the doubly indirect block has not been instantiated yet, allocate it */
// 				if(inode->data.doubly_indirect_block == NULL) {
// 					if(!free_map_allocate(1, &new_sector)) {
// 						PANIC("Couldn't allocate a new sector for case 2 for the doubly_indirect_block\n");
// 					}
//
// 					block_write(fs_device, new_sector, zeros);
// 					inode->data.doubly_indirect_block = &new_sector;
// 				}
//
// 				/* If the singly indirect block has not been instantiated yet, allocate it */
// 				if(inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index] == NULL) {
// 					if(!free_map_allocate(1, &new_sector)) {
// 						PANIC("Couldn't allocate a new sector for case 2 for the singly_indirect_blocks for the doubly_indirect_block");
// 					}
//
// 					block_write(fs_device, new_sector, zeros);
// 					inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index] = &new_sector;
// 				}
//
// 				/* If the sector to write to did not exist, allocate it */
// 				if(inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index]->direct_blocks[direct_index] == NULL) {
// 					if(!free_map_allocate(1, &new_sector)) {
// 						PANIC("Couldn't allocate a new sector for case 2 for the direct_blocks in the singly_indirect_blocks in the doubly_indirect_block\n");
// 					}
//
// 					block_write(fs_device, new_sector, zeros);
// 					inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index]->direct_blocks[direct_index] = &new_sector;
// 				}
//
// 				sector_idx = inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index]->direct_blocks[direct_index];
// 				break;
// 			}
// 			default: {
// 				PANIC("\"case\" set to something other than 0, 1 or 2\n");
// 			}
// 		}
// 		////////////printf("Out of the switch statement\n");
// 		/* Sector to write, starting byte offset within sector. */
// 		block_sector_t sector_idx = byte_to_sector (inode, offset);
// 		int sector_ofs = offset % BLOCK_SECTOR_SIZE;
//
// 		/* Bytes left in inode, bytes left in sector, lesser of the two. */
// 		off_t inode_left = inode_length (inode) - offset;
// 		int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
// 		int min_left = inode_left < sector_left ? inode_left : sector_left;
//
// 		/* Number of bytes to actually write into this sector. */
// 		int chunk_size = size < min_left ? size : min_left;
// 		if (chunk_size <= 0) {
// 			break;
// 		}
//
// 		if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE) {
// 			/* Write full sector directly to disk. */
// 			////////////printf("Before block_write 2\n");
// 			block_write (fs_device, sector_idx, buffer + bytes_written);
// 			////////////printf("After block_write 2\n");
// 		} else {
// 			/* We need a bounce buffer. */
// 			if (bounce == NULL) {
// 				bounce = malloc (BLOCK_SECTOR_SIZE);
// 				if (bounce == NULL) {
// 					break;
// 				}
// 			}
//
// 			/* If the sector contains data before or after the chunk
// 			   we're writing, then we need to read in the sector
// 			   first.  Otherwise we start with a sector of all zeros. */
// 			if (sector_ofs > 0 || chunk_size < sector_left) {
// 				block_read (fs_device, sector_idx, bounce);
// 			} else {
// 				memset (bounce, 0, BLOCK_SECTOR_SIZE);
// 			}
// 			memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
// 			////////////printf("Before block_write 3\n");
// 			block_write (fs_device, sector_idx, bounce);
// 			////////////printf("After block_write 3\n");
// 		}
//
// 		/* Advance. */
// 		size -= chunk_size;
// 		offset += chunk_size;
// 		bytes_written += chunk_size;
//
// 		//Check if you are in a new section now
// 		//TODO: copied and pasted from read function, check to make sure logic is still correct
// 		//TODO: do I need to check for EOF or out of bounds? What happens if they request more than the EOF?
// 		//seems like the PDF said allow past EOF, still not sure about past max size (vulnerable to attacks)
// 		if(offset < 0) {
// 			PANIC("Should this happen?\n");
// 		}
// 		if(offset > 8517631) {
// 			PANIC("Should probably just terminate the call at this point (reading past the max file size)\n");
// 		}
// 		if(offset >= 63488 && offset < 129023) {
// 			section = 1;
// 		}
// 		else if(offset >= 129023 && offset <= 8517631) {
// 			section = 2;
// 		}
// 	}
// 	free (bounce);
//
// 	return bytes_written;
// }
//
//-------------------SECOND TRY---------------------------
/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t inode_write_at(struct inode* inode, const void* buffer_, off_t size, off_t offset) {
	////////////printf("Starting inode_write_at, writing from inode %d\n", inode->sector);
	//////printf("Starting write, at sector %d, length is %d\n", inode->sector, inode->data.length);
	// //TODO: For debugging only, remove this
	// int orig_size = size;
	const uint8_t* buffer = buffer_;
	off_t bytes_written = 0;
	uint8_t* bounce = malloc(BLOCK_SECTOR_SIZE);
	bool extending = false;	//If the write will extend the file
	bool zeroing = false;		//If sectors need to be zeroed between current EOF and first write

	int sector_idx = offset / BLOCK_SECTOR_SIZE;
	int EOF_index = inode->data.length / BLOCK_SECTOR_SIZE;
	off_t orig_offset = offset;

	if(bounce == NULL) {
		PANIC("Bounce is NULL :/\n");
	}

	if(inode->deny_write_cnt)
		return 0;

	/* Check if the write extends the file */
	//TODO: should I update inode->data.length here or later?
	//Proably later (at the end) in case not all bytes were written
	//But what if byte_to_sector needs it? Check in a bit (should only matter if not allocate_if_not_found)
	if((size + offset) > inode->data.length) {
		extending = true;
		////////printf("In extending, good! Offset is %d, size of write is %d, current sector index is %d\n", offset, size, sector_idx);
		////////printf("inode->sector: %d\n", inode->sector);

		/* Check if sectors need to be zeroed out */
		if(sector_idx > (EOF_index + 1))
			zeroing = true;
	}

	/* Zero out the sectors between the original EOF and the new write */
	//TODO: ALSO NEED TO ZERO OUT THE START OF THE NEW BLOCK!!!!
	//Might as well zero out the block to start with, then write...
	if(zeroing) {
		// block_sector_t index_to_zero = (inode->data.length / 512) + 1;
		// EOF_index++;
		block_sector_t last_to_zero = sector_idx - 1;

		for(; EOF_index <= last_to_zero; EOF_index++) {
			//////printf("Calling byte_to_sector in write in the zeroing block, pos is %d\n", EOF_index * BLOCK_SECTOR_SIZE);
			//printf("In the zeroing for loop, zeroing at byte %d\n", EOF_index * BLOCK_SECTOR_SIZE);
			byte_to_sector(inode, EOF_index * BLOCK_SECTOR_SIZE, true);//TODO: Is this the correct calculation?
			////////printf("Zeroing bitches! Offset is %d, size of write is %d, current sector index is %d\n", offset, size, EOF_index);
		}
	}

	// int temp = 1;
	while(size > 0) {
		sector_idx = offset / BLOCK_SECTOR_SIZE;
		block_sector_t sector_to_write = -1;

		//TODO: THIS IS NOT GOOD ENOUGH!! May be false, length ends in a half sector
		//TODO: FIX THIS DAMNIT!!!
		//TODO: Should I also add size?
		//TODO: could take this out of the while loop (I think)
		/* If you are either in or past the last allocated sector */

		if(!extending) {
			//////printf("Calling byte_to_sector in write, not extending, pos is %d\n", offset);
			sector_to_write = byte_to_sector(inode, offset, false);
			if(sector_to_write == -1)
				PANIC("Not extending, but sector not allocated, fix this! This will be difficult...\n");
		}
		else {
			//Make sure to write all intermediary blocks that are returned from byte_to_sector
			/* Allocate blocks between previous EOF and new sector and write with 0s */
			//////printf("Calling byte_to_sector in write, extending, pos is %d\n", offset);
			sector_to_write = byte_to_sector(inode, offset, true);
			if(sector_to_write == -1)
				PANIC("Extending, but got back -1 from byte_to_sector");
		}
		////////////printf("sector_to_write: %d\n", sector_to_write);

		/* Write the data */
		int sector_ofs = offset % BLOCK_SECTOR_SIZE;
		int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
		int chunk_size = size < sector_left ? size : sector_left;	//If size < sector left, write the entire sector

		if(sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE) {
			block_write(fs_device, sector_to_write, buffer + bytes_written);
		}
		else {
			if(sector_ofs > 0 || chunk_size < sector_left)
				block_read(fs_device, sector_to_write, bounce);
			else
				memset(bounce, 0, BLOCK_SECTOR_SIZE);
			memcpy(bounce + sector_ofs, buffer + bytes_written, chunk_size);
			block_write(fs_device, sector_to_write, bounce);
		}
		/* Write the block that contained the sector written to */
		//TODO: actually, I don't think I need this, should only do it after allocating

		/* Advance */
		size -= chunk_size;
		offset += chunk_size;
		bytes_written += chunk_size;

		//TODO: Check if this is right (offset may be off by 1, or in the wrong block, or...)
		if(extending) {
			//////printf("Previous length: %d\n", inode->data.length);
			inode->data.length = (offset > inode->data.length) ? offset : inode->data.length;
			//////printf("New length: %d\n", inode->data.length);
			// inode->data.length += chunksize
			////////printf("inode->data.length after extension is: %d\n", inode->data.length);
		}
	}
	//TODO: After done writing, we don't know what case we were in. Need to write
	//ALL pointers to disk, because any of them might have been updated
	//Shouldn't byte_to_sector take care of that though? Should only ever be writing to direct blocks in this function

	if(bounce) {
		//free(bounce);
	}

	/* If there were blocks zeroed, need to update the offset again.
	   Doing this earlier wouldn't work because offset is used to know where
		 in the buffer to write */
	if(extending) {
		bytes_written = offset - orig_offset;
	}
	//TODO: remove the other length updates
	inode->data.length = inode->data.length > (orig_offset + bytes_written) ? inode->data.length : (orig_offset + bytes_written);
	// inode->data.length = orig_offset + bytes_written;
	// //printf("length is %d\n", inode->data.length);
	/* Need to write to disk so the file length is updated */
	block_write(fs_device, inode->sector, &inode->data);

	// if(inode->sector == 229) {
	// 	//printf("Writing %d bytes starting at offset %d to inode 229\n", orig_size, offset);
	// 	//printf("bytes_written: %d", bytes_written);
	// 	//printf("New length: inode->data.length%d\n", inode->data.length);
	// 	//printf("inode->data.direct_blocks[0]: %d\n", inode->data.direct_blocks[0]);
	// 	if(zeroing)
	// 		//printf("We were zeroing\n");
	// 	else
	// 		//printf("We were not zeroing\n");
	// 	// PANIC("Quit here\n");
	// }

	return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode* inode) {
	////////////printf("Starting inode_deny_write\n");
	inode->deny_write_cnt++;
	ASSERT (inode->deny_write_cnt <= inode->open_cnt);
	////////////printf("Finished inode_deny_write\n");
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode* inode) {
	////////////printf("Starting inode_allow_write\n");
	ASSERT (inode->deny_write_cnt > 0);
	ASSERT (inode->deny_write_cnt <= inode->open_cnt);
	inode->deny_write_cnt--;
	////////////printf("Starting inode_allow_write\n");
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode* inode) {
	////////////printf("Starting inode_length\n");
	////////////printf("Finished inode_length\n");
	return inode->data.length;
}
