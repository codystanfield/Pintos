#include "kernel/syscall.h"
#include "kernel/pagedir.h"
#include "kernel/process.h"
#include <stdio.h>
#include <stdbool.h>
#include <syscall-nr.h>
#include <string.h>
#include "kernel/thread.h"
#include "kernel/interrupt.h"
#include "kernel/vaddr.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

#include "filesys/directory.h"
#include "filesys/inode.h"

static void syscall_handler (struct intr_frame*);

void
syscall_init (void) {
	intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

bool is_valid_ptr (void* ptr);
void halt (struct intr_frame* f);
void exit (struct intr_frame* f);
void exec (struct intr_frame* f);
void wait (struct intr_frame* f);
void create (struct intr_frame* f);
void remove (struct intr_frame* f);
void open (struct intr_frame* f);
void filesize (struct intr_frame* f);
void read (struct intr_frame* f);
void write (struct intr_frame* f);
void seek (struct intr_frame* f);
void tell (struct intr_frame* f);
void close (struct intr_frame* f);

static void
syscall_handler (struct intr_frame* f) {
	int* syscall_num = (int*) (f->esp);
	if (!is_valid_ptr ((void*) syscall_num)) {
		thread_exit ();
	}

	switch (*syscall_num) {
	case SYS_HALT:        /* Halt the operating system. */
		halt (f);
		break;
	case SYS_EXIT:        /* Terminate this process. */
		exit (f);
		break;
	case SYS_EXEC:        /* Start another process. */
		exec (f);
		break;
	case SYS_WAIT:        /* Wait for a child process to die. */
		wait (f);
		break;
	case SYS_CREATE:      /* Create a file. */
		create (f);
		break;
	case SYS_REMOVE:      /* Delete a file. */
		remove (f);
		break;
	case SYS_OPEN:        /* Open a file. */
		open (f);
		break;
	case SYS_FILESIZE:    /* Obtain a file's size. */
		filesize (f);
		break;
	case SYS_READ:        /* Read from a file. */
		read (f);
		break;
	case SYS_WRITE:       /* Write to a file. */
		write (f);
		break;
	case SYS_SEEK:        /* Change position in a file. */
		seek (f);
		break;
	case SYS_TELL:        /* Report current position in a file. */
		tell (f);
		break;
	case SYS_CLOSE:       /* Close a file. */
		close (f);
		break;
	}
}

/* Checks if a pointer is valid by making sure the pointer isn't NULL, is within the user memory, and mapped to a existing page. */
bool
is_valid_ptr (void* ptr) {
	if (ptr == NULL) {
		return false;
	}
	if (!is_user_vaddr (ptr)) {
		return false;
	}
	if (pagedir_get_page (thread_current ()->pagedir, ptr) == NULL) {
		return false;
	}
	return true;
}

/* Calls shutdown_power_off which terminates the kernel. */
void
halt (struct intr_frame* f) {
	int* syscall_num = (int*) (f->esp);
	ASSERT (*syscall_num == SYS_HALT);

	shutdown_power_off ();
}


/* Exits a process (thread) with the given exit status. */
void
exit (struct intr_frame* f) {
	int* syscall_num = (int*) (f->esp);
	ASSERT (*syscall_num == SYS_EXIT);

	int* exit_status = syscall_num + 1;
	if (is_valid_ptr ((void*) exit_status)) {
		thread_current ()->exit_status = *exit_status;
	}

	thread_exit ();
}

/* Executes the process by forking a child process (thread) which will load the proper executable.
    Returns the PID (TID) of the child if it was loaded successfully otherwise, -1. */
void
exec (struct intr_frame* f) {
	int* syscall_num = (int*) (f->esp);
	ASSERT (*syscall_num == SYS_EXEC);

	char** cmdline = (char**) (syscall_num + 1);
	if (!is_valid_ptr ((void*) cmdline) ||
	    !is_valid_ptr ((void*) *cmdline)) {
		thread_exit ();
	}

	int pid = process_execute (*cmdline);
	f->eax = thread_current ()->exec_child_success ? pid : TID_ERROR;
}

/* Causes the parent thread to wait for a specified child until it finishes it's execution and reaps a zombie child.
    Returns exit status of the reaped child.*/
void
wait (struct intr_frame* f) {
	int* syscall_num = (int*) (f->esp);
	ASSERT (*syscall_num == SYS_WAIT);

	tid_t* child_tid = (tid_t*) (syscall_num + 1);
	if (!is_valid_ptr ((void*) child_tid)) {
		thread_exit ();
	}

	f->eax = process_wait (*child_tid);
}


/* Creates a file given the specified size and name.
    Returns whether the creation of the file was successful or not.*/
void
create (struct intr_frame* f) {
	lock_acquire (&thread_filesys_lock);
	int* syscall_num = (int*) (f->esp);
	ASSERT (*syscall_num == SYS_CREATE);

	char** name = (char**) (syscall_num + 1);
	unsigned* initial_size = (unsigned*) (syscall_num + 2);
	if (!is_valid_ptr ((void*) name) ||
	    !is_valid_ptr ((void*) *name) ||
	    !is_valid_ptr ((void*) initial_size)) {
		lock_release (&thread_filesys_lock);
		thread_exit ();
	}

	f->eax = filesys_create (*name, *initial_size);
	lock_release (&thread_filesys_lock);
}

/* Removes a file given a file name.
    Returns whether the deletion of a file is successful or not.

	 Must be updated so it can delete empty directories. */
void
remove (struct intr_frame* f) {
	lock_acquire (&thread_filesys_lock);
	int* syscall_num = (int*) (f->esp);
	ASSERT (*syscall_num == SYS_REMOVE);

	char** name = (char**) (syscall_num + 1);
	if (!is_valid_ptr ((void*) name) ||
	    !is_valid_ptr ((void*) *name)) {
		lock_release (&thread_filesys_lock);
		thread_exit ();
	}

	// if isdir()
	// do a different thing
	// dir_remove()

	f->eax = filesys_remove (*name);
	lock_release (&thread_filesys_lock);
}

/* Opens a file given it's name. If the file isn't NULL, the file is placed into the
    first open slot in open_files array within the current thread.
    Returns file descriptor for the newly opened file or -1 for invalid file. */
void
open (struct intr_frame* f) {
	lock_acquire (&thread_filesys_lock);
	int* syscall_num = (int*) (f->esp);
	ASSERT (*syscall_num == SYS_OPEN);

	char** name = (char**) (syscall_num + 1);
	if (!is_valid_ptr ((void*) name) ||
	    !is_valid_ptr ((void*) *name)) {
		lock_release (&thread_filesys_lock);
		thread_exit ();
	}

	// if isdir()
	// do a thing

	struct file* file = filesys_open (*name);

	if (file == NULL) {
		f->eax = -1;
		lock_release (&thread_filesys_lock);
		return;
	}

	struct thread* curr = thread_current ();
	int i;
	for (i = 0; i < MAX_FILES; i++)
		if (curr->open_files[i].used == 0) {
			break;
		}

	if (i < MAX_FILES) {
		curr->open_files[i].used = 1;
		curr->open_files[i].file = file;
		curr->open_files[i].fd = i + 2;
		f->eax = curr->open_files[i].fd;
	} else {
		f->eax = -1;
	}

	lock_release (&thread_filesys_lock);
}

/* Checks the size of a file given it's file descriptor by iterating thorugh the current
    thread's open_files list. If an invalid file descriptor is given, the thread is killed.
    Returns the size of the file. */
void
filesize (struct intr_frame* f) {
	lock_acquire (&thread_filesys_lock);
	int* syscall_num = (int*) (f->esp);
	ASSERT (*syscall_num == SYS_FILESIZE);

	int* fd = syscall_num + 1;
	if (!is_valid_ptr ((void*) fd)) {
		lock_release (&thread_filesys_lock);
		thread_exit ();
	}

	struct thread* curr = thread_current ();
	int i;
	for (i = 0; i < MAX_FILES; i++) {
		if (curr->open_files[i].used == 1 && curr->open_files[i].fd == *fd) {
			ASSERT (curr->open_files[i].file != NULL);
			f->eax = file_length (curr->open_files[i].file);
			lock_release (&thread_filesys_lock);
			return;
		}
	}

	/* Only reaches here in the case of an error. */
	lock_release (&thread_filesys_lock);
	thread_exit ();
}

/* Reads the a file given the file descriptor into a buffer of a given size.
    Checks if the file descriptor is standard input and if it isn't, checks the current thread's open_files.
    Returns the amount of characters read. */
void
read (struct intr_frame* f) {
	lock_acquire (&thread_filesys_lock);
	int* syscall_num = (int*) (f->esp);
	ASSERT (*syscall_num == SYS_READ);

	int* fd = syscall_num + 1;
	char** buffer = (char**) (syscall_num + 2);
	unsigned* size = (unsigned*) (syscall_num + 3);

	if (!is_valid_ptr ((void*) fd) ||
	    !is_valid_ptr ((void*) buffer) ||
	    !is_valid_ptr ((void*) *buffer) ||
	    !is_valid_ptr ((void*) size)) {
		lock_release (&thread_filesys_lock);
		thread_exit ();
	}

	if (*fd == STDIN_FILENO) {
		char c;
		c = input_getc ();
		memcpy(*buffer, &c, 1);
		f->eax = 1;
		lock_release (&thread_filesys_lock);
		return;
	}

	struct thread* curr = thread_current ();
	int i;
	for (i = 0; i < MAX_FILES; i++) {
		if (curr->open_files[i].used == 1 && curr->open_files[i].fd == *fd) {
			ASSERT (curr->open_files[i].file != NULL);
			f->eax = file_read (curr->open_files[i].file, *buffer, *size);
			lock_release (&thread_filesys_lock);
			return;
		}
	}

	/* Only reaches here in the case of an error. */
	lock_release (&thread_filesys_lock);
	thread_exit ();
}

/* Writes to the file given the file descriptor to a file from a buffer.
    Checks if the file descriptor is standard output and if it isn't, checks the current thread's open_files.
    Returns the amount of characters written. */
void
write (struct intr_frame* f) {
	lock_acquire (&thread_filesys_lock);
	int* syscall_num = (int*) (f->esp);
	ASSERT (*syscall_num == SYS_WRITE);

	int* fd = syscall_num + 1;
	char** buffer = (char**) (syscall_num + 2);
	unsigned* size = (unsigned*) (syscall_num + 3);

	if (!is_valid_ptr ((void*) fd) ||
	    !is_valid_ptr ((void*) buffer) ||
	    !is_valid_ptr ((void*) *buffer) ||
	    !is_valid_ptr ((void*) size)) {
		lock_release (&thread_filesys_lock);
		thread_exit ();
	}

	if (*fd == STDOUT_FILENO) {
		putbuf (*buffer, *size);
		f->eax = *size;
		lock_release (&thread_filesys_lock);
		return;
	}

	struct thread* curr = thread_current ();
	int i;
	for (i = 0; i < MAX_FILES; i++) {
		if (curr->open_files[i].used == 1 && curr->open_files[i].fd == *fd) {
			ASSERT (curr->open_files[i].file != NULL);
			f->eax = file_write (curr->open_files[i].file, *buffer, *size);
			lock_release (&thread_filesys_lock);
			return;
		}
	}

	/* Only reaches here in the case of an error. */
	lock_release (&thread_filesys_lock);
	thread_exit ();
}

/* Changes the next byte to be read or written in open file fd to position,
  	expressed in bytes from the beginning of the file. (Thus, a position of 0 is the file's start.) . */
void
seek (struct intr_frame* f) {
	lock_acquire (&thread_filesys_lock);
	int* syscall_num = (int*) (f->esp);
	ASSERT (*syscall_num == SYS_SEEK);

	int* fd = syscall_num + 1;
	unsigned* position = (unsigned*) (syscall_num + 2);

	if (!is_valid_ptr ((void*) fd) ||
	    !is_valid_ptr ((void*) position)) {
		lock_release (&thread_filesys_lock);
		thread_exit ();
	}

	struct thread* curr = thread_current ();
	int i;
	for (i = 0; i < MAX_FILES; i++) {
		if (curr->open_files[i].used == 1 && curr->open_files[i].fd == *fd) {
			ASSERT (curr->open_files[i].file != NULL);
			file_seek (curr->open_files[i].file, *position);
			lock_release (&thread_filesys_lock);
			return;
		}
	}

	/* Only reaches here in the case of an error. */
	lock_release (&thread_filesys_lock);
	thread_exit ();
}

/* Returns the position of the next byte to be read or
   	written in open file fd, expressed in bytes from the beginning of the file. */
void
tell (struct intr_frame* f) {
	lock_acquire (&thread_filesys_lock);
	int* syscall_num = (int*) (f->esp);
	ASSERT (*syscall_num == SYS_TELL);

	int* fd = syscall_num + 1;
	if (!is_valid_ptr ((void*) fd)) {
		lock_release (&thread_filesys_lock);
		thread_exit ();
	}

	struct thread* curr = thread_current ();
	int i;
	for (i = 0; i < MAX_FILES; i++) {
		if (curr->open_files[i].used == 1 && curr->open_files[i].fd == *fd) {
			ASSERT (curr->open_files[i].file != NULL);
			f->eax = file_tell (curr->open_files[i].file);
			lock_release (&thread_filesys_lock);
			return;
		}
	}

	/* Only reaches here in the case of an error. */
	lock_release (&thread_filesys_lock);
	thread_exit ();
}

/* Closes file descriptor fd. Exiting or terminating a process implicitly
   	closes all its open file descriptors, as if by calling this function for each one. */
void
close (struct intr_frame* f) {
	lock_acquire (&thread_filesys_lock);
	int* syscall_num = (int*) (f->esp);
	ASSERT (*syscall_num == SYS_CLOSE);

	int* fd = syscall_num + 1;
	if (!is_valid_ptr ((void*) fd)) {
		lock_release (&thread_filesys_lock);
		thread_exit ();
	}

	struct thread* curr = thread_current ();
	int i;
	for (i = 0; i < MAX_FILES; i++) {
		if (curr->open_files[i].used == 1 && curr->open_files[i].fd == *fd) {
			ASSERT (curr->open_files[i].file != NULL);
			file_close (curr->open_files[i].file);
			curr->open_files[i].used = 0;
			lock_release (&thread_filesys_lock);
			return;
		}
	}

	/* Only reaches here in the case of an error. */
	lock_release (&thread_filesys_lock);
	thread_exit ();
}

/* Changes the current working directory of the process to dir, which may be relative
	 	or absolute. Returns true if successful, false on failure. */

bool chdir (const char *dir) {
	lock_acquire (&thread_filesys_lock);

	struct inode **in;
	char *temp;
	char copy[128];
	char *saveptr;
	char *saveptr2;
	char *forwardslash = "/";
	char *space = " ";

	size_t n = strlcpy(copy, dir, sizeof(copy));

	// if the char is too long, not sure what else to do about this
	if (n>=sizeof(copy)) {
		//puts("Too long");
		return FALSE;
	}

	// get rid of any whitespace at the beginning
	char *token = strtok_r(copy, space, &saveptr);

	// absolute address
	if (strncmp(token, forwardslash, 1) == 0) {
		process->current_directory = dir_open_root();
	}
	char *token2 = strtok_r(token, forwardslash, &saveptr2);

	while (token2) {
		temp = token2;

		// if it's a directory
		if(dir_lookup(process->current_directory, temp, &in)) {
			// not correct syntax, but gets the point across
			process->current_directory = open(temp);
		}
		// if it's not a directory
		else {
			// if it isn't a valid directory, or is a file
			return FALSE;
		}
		token2 = strtok_r(NULL, forwardslash, &saveptr2);
	}

	// if relative address
			// if dir is an existing directory, set current directory to dir
			// if dir isn't an existing directory, return false
	// if absolute address (/dir/dir/dir)
			// tokenize char array with '/' as the delimiter, search for dir in home
					// if dir doesn't exist, return false
					// if dir does exist, get next directory, repeat process
						// if next char is null terminator, return true

	lock_release(&thread_filesys_lock);
	return TRUE;
}

/* Creates the directory named dir, which may be relative or absolute. Returns true if
	 	successful, false on failure. Fails if dir already exists or if any directory name in dir,
	 	besides the last, does not already exist. That is, mkdir("/a/b/c") succeeds only if
	 	‘/a/b’ already exists and ‘/a/b/c’ does not. */

bool mkdir (const char *dir) {
	lock_acquire (&thread_filesys_lock);

	// given a call like mkdir("a/b/c")
	// tokenize copy using whitespace
		// a/b/c
		// /d/e/f
		// /g/h/
		// check if first char is "/"
			// if yes, absolute address
			// if no, relative address

	// if given an absolute address
			// tokenize the char arrays, using '/' as the delimiter, and test each part to see if the name exists and is a directory, not file
			// if it doesn't already exist, check if the next piece is the null terminator
					// if not, return false
					// if yes, make the new directory by assiging the name an inode
			// if it does exist, retry until it is or until null terminator
	// if given relative address
			// check if name is already used
					// if it is, return false
					// if not, make the new directory by assigning the name an inode

	// maybe like this? to keep track of what directory you're looking from
	// without changing your actual directory
	// or I could use it to keep track of the starting directory, then
	// use chdir(dir) to cycle through
	struct dir* temp_dir = process->current_directory;

	bool success = FALSE;
	char *temp;
	char copy[128];
	char *saveptr;
	char *saveptr2;
	char *forwardslash = "/";
	char *space = " ";

	size_t n = strlcpy(copy, dir, sizeof(copy));

	// if the char is too long, not sure what else to do about this
	if (n>=sizeof(copy)) {
		//puts("Too long");
		return FALSE;
	}

	// only need to tokenize once because mkdir will only have 1 non-whitespace part at a time
	char *token = strtok_r(copy, space, &saveptr);

	// absolute address
	if (strncmp(token, forwardslash, 1) == 0) {
		// if it fails for whatever reason
		if (!chdir("/")) {
			return FALSE;
		}
	}

	char *token2 = strtok_r(token, forwardslash, &saveptr2);

	while (token2) {
		temp = token2;
		token2 = strtok_r(NULL, forwardslash, &saveptr2);
		success = chdir(temp);

		if (!token2 && !success) {
			// if cannot change to temp2 and there are no more things to parse
			return dir_create(, 2) ? TRUE : FALSE;
		}
		else if ((!token2 && success) || (token2 && !success)) {
			// if there's nothing left to parse and successfully changed to temp directory
			// (means failure because directory already exists)
			// or more to parse but invalid directory switch
			return FALSE;
		}
		// else { more things to parse }
	}

	lock_release(&thread_filesys_lock);
	return TRUE;
}

/* Reads a directory entry from file descriptor fd, which must represent a directory. If
	 	successful, stores the null-terminated file name in name, which must have room for
	 	READDIR_MAX_LEN + 1 bytes, and returns true. If no entries are left in the directory,
	 	returns false.

	 ‘.’ and ‘..’ should not be returned by readdir.
	 If the directory changes while it is open, then it is acceptable for some entries not to
	 	be read at all or to be read multiple times. Otherwise, each directory entry should
	 	be read once, in any order.

	 READDIR_MAX_LEN is defined in ‘lib/user/syscall.h’. If your file system supports
	 	longer file names than the basic file system, you should increase this value from the
	 	default of 14. */

bool readdir (int fd, char *name) {
	lock_acquire (&thread_filesys_lock);

	if (!isdir(fd)) {
		return FALSE;
	}

	for (int i = 0; i<MAX_FILES; i++) {
		if (curr->open_files[i].fd == fd) {
			//return curr->open_files[i].inode->sector;
			return dir_readdir(dir_open(curr->open_files[i].file->inode), &name);
		}
	}

	lock_release(&thread_filesys_lock);
	return FALSE;
}

/* Returns true if fd represents a directory, false if it represents an ordinary file. */

bool isdir (int fd) {
	for (int i = 0; i<MAX_FILES; i++) {
		if (curr->open_files[i].fd == fd) {
			return curr->open_files[i].file->inode->is_a_directory;
		}
	}
	return FALSE;
}

/* Returns the inode number of the inode associated with fd, which may represent an
	  ordinary file or a directory.

	 An inode number persistently identifies a file or directory. It is unique during the
	  file’s existence. In Pintos, the sector number of the inode is suitable for use as an
	  inode number. */

int inumber (inf fd) {

	// if valid file directory
			// get inode number

	// goes through the list of currently open files
	for (int i = 0; i<MAX_FILES; i++) {
		if (curr->open_files[i].fd == fd) {
			return curr->open_files[i].file->inode->sector;
		}
	}
	return -1;
}
