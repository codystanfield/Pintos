#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "kernel/malloc.h"


/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk {
	block_sector_t start;               /* First data sector. */
	off_t length;                       /* File size in bytes. */
	unsigned magic;                     /* Magic number. */
	uint32_t* direct_blocks[123];						/* Array of pointers to direct blocks; addresses 61.5kB, or 62,976 bytes */
	singly_indirect_block* singly_indirect_block_;	/* Pointer to block that contains array of pointers to direct blocks  (128 pointers total); addresses 64kB, or 65,536 bytes */
	doubly_indirect_block* doubly_indirect_block_;	/* Pointer to block that contains array of pointers to singly indirect blocks (128 pointers to singly indirect blocks, totals 16384 data blocks); addresses 8MB, or 8,388,608 bytes */
};

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size) {
	return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode {
	struct list_elem elem;              /* Element in inode list. */
	block_sector_t sector;              /* Sector number of disk location. */
	int open_cnt;                       /* Number of openers. */
	bool removed;                       /* True if deleted, false otherwise. */
	int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
	struct inode_disk data;             /* Inode content. */
};

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode* inode, off_t pos) {
	ASSERT (inode != NULL);
	if (pos < inode->data.length) {
		return inode->data.start + pos / BLOCK_SECTOR_SIZE;
	} else {
		return -1;
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
//TODO: need to update for new inode structure. Make sure to have the pointers point to NULL to keep file size down while you can
bool
inode_create (block_sector_t sector, off_t length) {
	struct inode_disk* disk_inode = NULL;
	bool success = false;

	ASSERT (length >= 0);

	/* If this assertion fails, the inode structure is not exactly
	   one sector in size, and you should fix that. */
	ASSERT (sizeof * disk_inode == BLOCK_SECTOR_SIZE);

	disk_inode = calloc (1, sizeof * disk_inode);
	if (disk_inode != NULL) {
		size_t sectors = bytes_to_sectors (length);
		disk_inode->length = length;
		disk_inode->magic = INODE_MAGIC;
		if (free_map_allocate (sectors, &disk_inode->start)) {
			block_write (fs_device, sector, disk_inode);
			if (sectors > 0) {
				static char zeros[BLOCK_SECTOR_SIZE];
				size_t i;
				//TODO: I think this initializes the pointers to NULL (since everything is zeroed out)
				for (i = 0; i < sectors; i++) {
					block_write (fs_device, disk_inode->start + i, zeros);
				}
			}
			success = true;
		}
		free (disk_inode);
	}
	return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
//TODO: need to check how much memory to allocate depenging on file size
struct inode*
inode_open (block_sector_t sector) {
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
//TODO: update to make sure to free allocated pointers if necessary
void
inode_close (struct inode* inode) {
	/* Ignore null pointer. */
	if (inode == NULL) {
		return;
	}

	/* Release resources if this was the last opener. */
	if (--inode->open_cnt == 0) {
		/* Remove from inode list and release lock. */
		list_remove (&inode->elem);

		/* Deallocate blocks if removed. */
		if (inode->removed) {
			free_map_release (inode->sector, 1);
			free_map_release (inode->data.start,
			                  bytes_to_sectors (inode->data.length));
		}

		free (inode);
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

/* TODO: could be optimized greatly by not traversing the structure after each sector */
off_t inode_read_at(struct inode* inode, void* buffer_, off_t size, off_t offset) {
	uint8_t* buffer = buffer_;
	off_t bytes_read = 0;
	uint8_t* bounce = NULL;

	int section;
	uint32_t* sector_idx;
	int sector_ofs;

	if(offset < 0) {
		PANIC("You dun goofed, reading from a negative address relative to the file\n");
	}
	//TODO: Should this simply read as much of the file as possible and then return instead of panicking?
	if(offset > 8517120) {
		PANIC("You dun goofed, reading outside of the max file size (~8MB)\n");
	}
	if(offset < 62976) {
		section = 0;
	}
	else if(offset < 128512) {
		section = 1;
	}
	else {
		section = 2;
	}

	while(size > 0) {
		//need brackets on each case for some reason, compile error without it (has to do with declaring a variable as the first statement in a case)
		switch(section) {
			/* Accessing direct blocks */
			case 0: {
				int direct_index = offset / BLOCK_SECTOR_SIZE;	//TODO: byte_to_sector() perhaps?
				//should this be a block_sector_t and use byte_to_sector?
				sector_idx = inode->data.direct_blocks[direct_index];	//pointer to location of the sector
				break;
			}
			/* Accessing singly indirect blocks */
			case 1: {
				int direct_index = (offset - 62976) / BLOCK_SECTOR_SIZE;
				sector_idx = inode->data.singly_indirect_block_->direct_blocks[direct_index];
			}
			/* Accessing doubly indirect blocks */
			case 2: {
				int indirect_index = (offset - 128512) / (BLOCK_SECTOR_SIZE * 128);
				int direct_index = indirect_index / BLOCK_SECTOR_SIZE;
				sector_idx = inode->data.doubly_indirect_block_->singly_indirect_blocks[indirect_index]->direct_blocks[direct_index];
			}
			/* Something went wrong */
			default: {
				PANIC("\"case\" set to something other than 0, 1 or 2\n");
			}
		}

		if(sector_idx == NULL) {
			PANIC("Accessing uninitialized sector (should probably just return here)\n");
		}

		sector_ofs = offset % BLOCK_SECTOR_SIZE;

		off_t sector_left = BLOCK_SECTOR_SIZE - sector_ofs;

		int chunk_size;
		if(size < sector_left) {
			chunk_size = size;
		}
		else {
			chunk_size = sector_left;
		}

		if(chunk_size <= 0) {
			break;
		}

		if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE) {
			/* Read full sector directly into caller's buffer. */
			block_read (fs_device, sector_idx, buffer + bytes_read);
		} else {
			/* Read sector into bounce buffer, then partially copy
			   into caller's buffer. */
			if (bounce == NULL) {
				bounce = malloc (BLOCK_SECTOR_SIZE);
				if (bounce == NULL) {
					break;
				}
			}
			block_read (fs_device, sector_idx, bounce);	//read the sector into the bounce buffer
			memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size); //read chunk_size bytes from bounce to the buffer
		}

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_read += chunk_size;

		//Check if you are in a new section now
		//TODO: do I need to check for EOF or out of bounds? What happens if they request more than the EOF?
		//seems like the PDF said allow past EOF, still not sure about past max size (vulnerable to attacks)
		if(offset < 0) {
			PANIC("Should this happen?\n");
		}
		if(offset > 8517120) {
			PANIC("Should probably just terminate the call at this point (reading past the max file size)\n");
		}
		if(offset >= 62976 && offset < 128512) {
			section = 1;
		}
		else if(offset >= 128512 && offset <= 8517120) {
			section = 2;
		}
	}
	free(bounce);

	return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
//TODO: update to grow file when you run out of the current type of blocks (direct, singly indirect, doubly indirect)
off_t
inode_write_at (struct inode* inode, const void* buffer_, off_t size,
                off_t offset) {
	const uint8_t* buffer = buffer_;
	off_t bytes_written = 0;
	uint8_t* bounce = NULL;

	int section;
	uint32_t* sector_idx;
	int sector_ofs;

	if (inode->deny_write_cnt) {
		return 0;
	}

	if(offset < 0) {
		PANIC("You dun goofed, writing to a negative address relative to the file\n");
	}
	//TODO: should probably just have this return at this point
	if(offset > 8517120) {
		PANIC("You dun goofed, writing outside of the max file size (~8MB)\n");
	}
	if(offset < 62976) {
		section = 0;
	}
	else if(offset < 128512) {
		section = 1;
	}
	else {
		section = 2;
	}

	//TODO: THIS IS WRONG!!! NEED TO CHECK FOR FREE SECTORS
	while (size > 0) {
		switch(section) {
			case 0: {
				int direct_index = offset / BLOCK_SECTOR_SIZE;
				//if the sector exists, set sector_idx to the sector, else allocate the sector
				sector_idx = inode->data.direct_blocks[direct_index];
				//TODO: check for NULL pointer, allocate if needed
			}
			case 1: {
				int direct_index = (offset - 62976) / BLOCK_SECTOR_SIZE;
				sector_idx = inode->data.singly_indirect_block_->direct_blocks[direct_index];
				//TODO: check for NULL pointer, allocate if needed
			}
			case 2: {
				int indirect_index = (offset - 128512) / (BLOCK_SECTOR_SIZE * 128));
				int direct_index = indirect_index / BLOCK_SECTOR_SIZE;
				sector_idx = inode->data.doubly_indirect_block_->singly_indirect_blocks[indirect_index]->direct_blocks[direct_index];
				//TODO: check for NULL pointer, allocate if needed
			}
		}

		/* Sector to write, starting byte offset within sector. */
		block_sector_t sector_idx = byte_to_sector (inode, offset);
		int sector_ofs = offset % BLOCK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length (inode) - offset;
		int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually write into this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0) {
			break;
		}

		if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE) {
			/* Write full sector directly to disk. */
			block_write (fs_device, sector_idx, buffer + bytes_written);
		} else {
			/* We need a bounce buffer. */
			if (bounce == NULL) {
				bounce = malloc (BLOCK_SECTOR_SIZE);
				if (bounce == NULL) {
					break;
				}
			}

			/* If the sector contains data before or after the chunk
			   we're writing, then we need to read in the sector
			   first.  Otherwise we start with a sector of all zeros. */
			if (sector_ofs > 0 || chunk_size < sector_left) {
				block_read (fs_device, sector_idx, bounce);
			} else {
				memset (bounce, 0, BLOCK_SECTOR_SIZE);
			}
			memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
			block_write (fs_device, sector_idx, bounce);
		}

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_written += chunk_size;
	}
	free (bounce);

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
