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

static inline size_t
bytes_to_sectors (off_t size) {
	return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
	 If allocate_if_not_found is set to true, will allocate a sector to the given
	 location and write the updated sectors to disk.
   If allocate_if_not_found is set to false, returns -1 if INODE does not contain
	 data for a byte at offset POS.
	 If allocating blocks, this function will also write them to the disk. This is
	 bad for performance, but much easier on the programmer to not have to worry
	 about what all has been allocated
	 Also, some of this code (such as the if/else blocks) is inneficient, but again,
	 readability and ease of use too precedence */
static block_sector_t byte_to_sector (struct inode* inode, off_t pos, bool allocate_if_not_found) {
	ASSERT (inode != NULL);

	/* If accessing a byte out of range and don't want to allocate, return -1 */
	if(pos >= inode->data.length && !allocate_if_not_found)
		return -1;

	/* If the requested byte is within the file size, find the corresponding sector */
	block_sector_t sector_idx = pos / BLOCK_SECTOR_SIZE;
	int section;	/*0 = direct, 1 = indirect, 2 = doubly indirect */
	static char zeros[BLOCK_SECTOR_SIZE];
	block_sector_t new_sector = -1;
	int i;

	/* Determine how much indirection is needed */
	if(sector_idx < FIRST_INDIRECT_BLOCK)
		section = 0;
	else if(sector_idx < FIRST_DOUBLY_INDIRECT_BLOCK)
		section = 1;
	else if(sector_idx < LAST_DOUBLY_INDIRECT_BLOCK + 1)
		section = 2;
	else {
		PANIC("In byte_to_sector, sector_idx out of bounds\n");
		return -1;
	}

	switch(section) {
		/* In the direct blocks */
		case 0: {
			if(inode->data.direct_blocks[sector_idx] != -1) {
				return inode->data.direct_blocks[sector_idx];
			}
			else if(allocate_if_not_found) {
				free_map_allocate(1, &new_sector);
				inode->data.direct_blocks[sector_idx] = new_sector;
				block_write(fs_device, inode->sector, &inode->data);
				block_write(fs_device, new_sector, zeros);

				return new_sector;
			}
			else {
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
				/* If the singly indirect block is not allocated */
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
				/* If the direct block is not allocated */
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

				/* Write the sectors */
				block_write(fs_device, inode->sector, &inode->data);
				block_write(fs_device, inode->data.doubly_indirect_block_sector, inode->data.doubly_indirect_block);
				block_write(fs_device, inode->data.doubly_indirect_allocations->indirect_block_sectors[indirect_index], inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index]);
				block_write(fs_device, inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index]->direct_blocks[direct_index], zeros);

				return inode->data.doubly_indirect_block->singly_indirect_blocks[indirect_index]->direct_blocks[direct_index];
			}

			break;
		}

		/* Should never actually get here, but compiler will throw a warning about
		   control reaching the end of a non-void function */
		default: {
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

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool inode_create(block_sector_t sector, off_t length, bool is_dir) {
	struct inode_disk* disk_inode;
	bool success = false;
	block_sector_t new_sector;	/* Not strictly necessary, makes code easier to read though */
	static char zeros[BLOCK_SECTOR_SIZE];
	int i, j;

	ASSERT(length >= 0);

	/* If this assertion fails, the inode structure is not exactly
	one sector in size, and you should fix that. */
	ASSERT(sizeof * disk_inode == BLOCK_SECTOR_SIZE);

	disk_inode = calloc(1, sizeof * disk_inode);
	if(disk_inode != NULL) {	/*If allocation of memory was successful */
		block_sector_t sector_idx = length / BLOCK_SECTOR_SIZE;
		int section = -1;

		disk_inode->length = length;
		disk_inode->magic = INODE_MAGIC;
		disk_inode->is_directory = is_dir ? true : false;
		disk_inode->parent_directory = -1;

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
				/* Allocate the singly indirect block */
				disk_inode->singly_indirect_block = calloc(1, BLOCK_SECTOR_SIZE);
				free_map_allocate(1, &new_sector);
				disk_inode->singly_indirect_block_sector = new_sector;

				/* Allocate the direct blocks */
				for(i = 0; i <= LAST_DIRECT_BLOCK; i++) {
					free_map_allocate(1, &new_sector);
					block_write(fs_device, new_sector, zeros);
					disk_inode->direct_blocks[i] = new_sector;
				}

				/* Allocate the singly indirect block */
				for(i = 0; i <= sector_idx - FIRST_INDIRECT_BLOCK; i++) {
					free_map_allocate(1, &new_sector);
					block_write(fs_device, new_sector, zeros);
					disk_inode->singly_indirect_block->direct_blocks[i] = new_sector;
				}

				/* Set the blocks not needed to -1 */
				for(i = sector_idx - FIRST_INDIRECT_BLOCK + 1; i <= LAST_INDIRECT_BLOCK; i++) {
					disk_inode->singly_indirect_block->direct_blocks[i] = -1;
				}

				/* Initiallize other blocks to NULL */
				disk_inode->doubly_indirect_block = NULL;
				disk_inode->doubly_indirect_allocations = NULL;
				disk_inode->doubly_indirect_block_sector = -1;
				disk_inode->doubly_indirect_allocations_sector = -1;

				block_write(fs_device, disk_inode->singly_indirect_block_sector, disk_inode->singly_indirect_block);
				//free(disk_inode->singly_indirect_block);
				disk_inode->singly_indirect_block = NULL;

				break;
			}

			/* Using the doubly indirect block */
			case 2: {
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
				int indirect_index = (sector_idx - FIRST_DOUBLY_INDIRECT_BLOCK) / 128;
				int direct_index = (sector_idx - FIRST_DOUBLY_INDIRECT_BLOCK) % 128;

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
	}

	//free(disk_inode);
	disk_inode = NULL;

	return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode* inode_open (block_sector_t sector) {
	struct list_elem* e;
	struct inode* inode;

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

/* Closes INODE and writes it to disk. (Does it?  Check code.)
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void inode_close(struct inode* inode) {
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

		//free(inode);
	}
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode* inode) {
	ASSERT (inode != NULL);
	inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t inode_read_at(struct inode* inode, void* buffer_, off_t size, off_t offset) {
	uint8_t* buffer = buffer_;
	off_t bytes_read = 0;
	uint8_t* bounce = NULL;

	while(size > 0) {
		/* Disk sector to read, starting byte offset within sector */
		block_sector_t sector_to_read = byte_to_sector(inode, offset, false);
		int sector_ofs = offset % BLOCK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two
		   This still works because the inode is seen as being contiguous, and a
			 read does not extend the file */
		off_t inode_left = inode->data.length - offset;
		int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually copy out of this sector */
		int chunk_size = size < min_left ? size : min_left;
		if(chunk_size <= 0)
			break;

		if(sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE) {
			/* Read full sector directly into caller's buffer */
			block_read(fs_device, sector_to_read, buffer + bytes_read);
		}
		else {
			/* Read sector into bounce buffer, then partially copy
			   into caller's buffer */
			if(bounce == NULL) {
				bounce = malloc(BLOCK_SECTOR_SIZE);
				if(bounce == NULL) {
					break;
				}
			}
			block_read(fs_device, sector_to_read, bounce);
			memcpy(buffer + bytes_read, bounce + sector_ofs, chunk_size);
		}

		/* Advance */
		size -= chunk_size;
		offset += chunk_size;
		bytes_read += chunk_size;
	}
	if(bounce) {
		//free(bounce);
		bounce = NULL;
	}

	return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t inode_write_at(struct inode* inode, const void* buffer_, off_t size, off_t offset) {
	const uint8_t* buffer = buffer_;
	off_t bytes_written = 0;
	uint8_t* bounce = malloc(BLOCK_SECTOR_SIZE);
	bool extending = false;	/* If the write will extend the file */
	bool zeroing = false;		/* If sectors need to be zeroed between current EOF and first write */

	int sector_idx = offset / BLOCK_SECTOR_SIZE;
	int EOF_index = inode->data.length / BLOCK_SECTOR_SIZE;
	off_t orig_offset = offset;

	if(bounce == NULL) {
		PANIC("Bounce is NULL, could not be allocated\n");
	}

	if(inode->deny_write_cnt)
		return 0;

	/* Check if the write extends the file */
	if((size + offset) > inode->data.length) {
		extending = true;

		/* Check if sectors need to be zeroed out */
		if(sector_idx > (EOF_index + 1))
			zeroing = true;
	}

	/* Zero out the sectors between the original EOF and the new write */
	if(zeroing) {
		block_sector_t last_to_zero = sector_idx - 1;

		for(; EOF_index <= last_to_zero; EOF_index++) {
			byte_to_sector(inode, EOF_index * BLOCK_SECTOR_SIZE, true);
		}
	}

	while(size > 0) {
		sector_idx = offset / BLOCK_SECTOR_SIZE;
		block_sector_t sector_to_write = -1;

		/* If you are either in or past the last allocated sector */
		if(!extending) {
			sector_to_write = byte_to_sector(inode, offset, false);
			if(sector_to_write == -1)
				PANIC("Not extending, but sector not allocated, fix this! This will be difficult...\n");
		}
		else {
			/* Allocate blocks between previous EOF and new sector and write with 0s */
			sector_to_write = byte_to_sector(inode, offset, true);
			if(sector_to_write == -1)
				PANIC("Extending, but got back -1 from byte_to_sector");
		}

		/* Write the data */
		int sector_ofs = offset % BLOCK_SECTOR_SIZE;
		int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
		int chunk_size = size < sector_left ? size : sector_left;	/*If size < sector left, write the entire sector */

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

		/* Advance */
		size -= chunk_size;
		offset += chunk_size;
		bytes_written += chunk_size;

	}

	if(bounce) {
		//free(bounce);
	}

	inode->data.length = inode->data.length > (orig_offset + bytes_written) ? inode->data.length : (orig_offset + bytes_written);

	/* Need to write to disk so the file length is updated */
	block_write(fs_device, inode->sector, &inode->data);

	return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode* inode) {
	inode->deny_write_cnt++;
	ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode* inode) {
	ASSERT (inode->deny_write_cnt > 0);
	ASSERT (inode->deny_write_cnt <= inode->open_cnt);
	inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode* inode) {
	return inode->data.length;
}
