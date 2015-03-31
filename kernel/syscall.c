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

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

bool is_valid_ptr (void *ptr);
void halt (struct intr_frame *f);
void exit (struct intr_frame *f);
void exec (struct intr_frame *f);
void wait (struct intr_frame *f);
void create (struct intr_frame *f);
void remove (struct intr_frame *f);
void open (struct intr_frame *f);
void filesize (struct intr_frame *f);
void read (struct intr_frame *f);
void write (struct intr_frame *f);
void seek (struct intr_frame *f);
void tell (struct intr_frame *f);
void close (struct intr_frame *f);

static void
syscall_handler (struct intr_frame *f) 
{
  int *syscall_num = (int *) (f->esp);
  if (!is_valid_ptr ((void *) syscall_num))
    thread_exit ();

  switch (*syscall_num)
    {
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
is_valid_ptr (void *ptr)
{
  if (ptr == NULL)
    return false;
  if (!is_user_vaddr (ptr))
    return false;
  if (pagedir_get_page (thread_current ()->pagedir, ptr) == NULL)
    return false;
  return true;
}

/* Calls shutdown_power_off which terminates the kernal. */
void
halt (struct intr_frame *f)
{
  int *syscall_num = (int *) (f->esp);
  ASSERT (*syscall_num == SYS_HALT);

  shutdown_power_off ();
}


/* Exits a process (thread) with the given exit status. */
void
exit (struct intr_frame *f)
{
  int *syscall_num = (int *) (f->esp);
  ASSERT (*syscall_num == SYS_EXIT);

  int *exit_status = syscall_num + 1;
  if (is_valid_ptr ((void *) exit_status))
    thread_current ()->exit_status = *exit_status;

  thread_exit ();
}

/* Executes the process by forking a child process (thread) which will load the proper executable. 
    Returns the PID (TID) of the child if it was loaded successfully otherwise, -1. */
void
exec (struct intr_frame *f)
{
  int *syscall_num = (int *) (f->esp);
  ASSERT (*syscall_num == SYS_EXEC);

  char **cmdline = (char **) (syscall_num + 1);
  if (!is_valid_ptr ((void *) cmdline) ||
      !is_valid_ptr ((void *) *cmdline))
    thread_exit ();

  int pid = process_execute (*cmdline);
  f->eax = thread_current ()->exec_child_success ? pid : TID_ERROR;
}

/* Causes the parent thread to wait for a specified child until it finishes it's execution and reaps a zombie child. 
    Returns exit status of the reaped child.*/
void
wait (struct intr_frame *f)
{
  int *syscall_num = (int *) (f->esp);
  ASSERT (*syscall_num == SYS_WAIT);

  tid_t *child_tid = (tid_t *) (syscall_num + 1);
  if (!is_valid_ptr ((void *) child_tid))
    thread_exit ();

  f->eax = process_wait (*child_tid);
}


/* Creates a file given the specified size and name. 
    Returns whether the creation of the file was successful or not.*/
void
create (struct intr_frame *f)
{
  lock_acquire (&thread_filesys_lock);
  int *syscall_num = (int *) (f->esp);
  ASSERT (*syscall_num == SYS_CREATE);

  char **name = (char **) (syscall_num + 1);
  unsigned *initial_size = (unsigned *) (syscall_num + 2);
  if (!is_valid_ptr ((void *) name) ||
      !is_valid_ptr ((void *) *name) ||
      !is_valid_ptr ((void *) initial_size))
    {
      lock_release (&thread_filesys_lock);
      thread_exit ();
    }

  f->eax = filesys_create (*name, *initial_size);
  lock_release (&thread_filesys_lock);
}

/* Removes a file given a file name.
    Returns whether the deletion of a file is successful or not. */
void
remove (struct intr_frame *f)
{
  lock_acquire (&thread_filesys_lock);
  int *syscall_num = (int *) (f->esp);
  ASSERT (*syscall_num == SYS_REMOVE);

  char **name = (char **) (syscall_num + 1);
  if (!is_valid_ptr ((void *) name) ||
      !is_valid_ptr ((void *) *name))
    {
      lock_release (&thread_filesys_lock);
      thread_exit ();
    }

  f->eax = filesys_remove (*name);
  lock_release (&thread_filesys_lock);
}

/* Opens a file given it's name. If the file isn't NULL, the file is placed into the 
    first open slot in open_files array within the current thread. 
    Returns file descriptor for the newly opened file or -1 for invalid file. */
void
open (struct intr_frame *f)
{
  lock_acquire (&thread_filesys_lock);
  int *syscall_num = (int *) (f->esp);
  ASSERT (*syscall_num == SYS_OPEN);

  char **name = (char **) (syscall_num + 1);
  if (!is_valid_ptr ((void *) name) ||
      !is_valid_ptr ((void *) *name))
    {
      lock_release (&thread_filesys_lock);
      thread_exit ();
    }

  struct file *file = filesys_open (*name);

  if (file == NULL)
    {
      f->eax = -1;
      lock_release (&thread_filesys_lock);
      return;
    }

  struct thread *curr = thread_current ();
  int i;
  for (i = 0; i < MAX_FILES; i++)
    if (curr->open_files[i].used == 0)
      break;

  if (i < MAX_FILES)
    {
      curr->open_files[i].used = 1;
      curr->open_files[i].file = file;
      curr->open_files[i].fd = i + 2;
      f->eax = curr->open_files[i].fd;
    }
  else
    f->eax = -1;

  lock_release (&thread_filesys_lock);
}

/* Checks the size of a file given it's file descriptor by iterating thorugh the current 
    thread's open_files list. If an invalid file descriptor is given, the thread is killed.
    Returns the size of the file. */
void
filesize (struct intr_frame *f)
{
  lock_acquire (&thread_filesys_lock);
  int *syscall_num = (int *) (f->esp);
  ASSERT (*syscall_num == SYS_FILESIZE);

  int *fd = syscall_num + 1;
  if (!is_valid_ptr ((void *) fd))
    {
      lock_release (&thread_filesys_lock);
      thread_exit ();
    }

  struct thread *curr = thread_current ();
  int i;
  for (i = 0; i < MAX_FILES; i++)
    {
      if (curr->open_files[i].used == 1 && curr->open_files[i].fd == *fd)
        {
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
read (struct intr_frame *f)
{
  lock_acquire (&thread_filesys_lock);
  int *syscall_num = (int *) (f->esp);
  ASSERT (*syscall_num == SYS_READ);

  int *fd = syscall_num + 1;
  char **buffer = (char **) (syscall_num + 2);
  unsigned *size = (unsigned *) (syscall_num + 3);

  if (!is_valid_ptr ((void *) fd) ||
      !is_valid_ptr ((void *) buffer) ||
      !is_valid_ptr ((void *) *buffer) ||
      !is_valid_ptr ((void *) size))
    {
      lock_release (&thread_filesys_lock);
      thread_exit ();
    }

  if (*fd == STDIN_FILENO)
    {
      char c;
      c = input_getc ();
      memcpy(*buffer, &c, 1);  
      f->eax = 1;
      lock_release (&thread_filesys_lock);
      return;
    }

  struct thread *curr = thread_current ();
  int i;
  for (i = 0; i < MAX_FILES; i++)
    {
      if (curr->open_files[i].used == 1 && curr->open_files[i].fd == *fd)
        {
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
write (struct intr_frame *f)
{
  lock_acquire (&thread_filesys_lock);
  int *syscall_num = (int *) (f->esp);
  ASSERT (*syscall_num == SYS_WRITE);

  int *fd = syscall_num + 1;
  char **buffer = (char **) (syscall_num + 2);
  unsigned *size = (unsigned *) (syscall_num + 3);

  if (!is_valid_ptr ((void *) fd) ||
      !is_valid_ptr ((void *) buffer) ||
      !is_valid_ptr ((void *) *buffer) ||
      !is_valid_ptr ((void *) size))
    {
      lock_release (&thread_filesys_lock);
      thread_exit ();
    }

  if (*fd == STDOUT_FILENO)
    {
      putbuf (*buffer, *size);
      f->eax = *size;
      lock_release (&thread_filesys_lock);
      return;
    }

  struct thread *curr = thread_current ();
  int i;
  for (i = 0; i < MAX_FILES; i++)
    {
      if (curr->open_files[i].used == 1 && curr->open_files[i].fd == *fd)
        {
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
seek (struct intr_frame *f)
{
  lock_acquire (&thread_filesys_lock);
  int *syscall_num = (int *) (f->esp);
  ASSERT (*syscall_num == SYS_SEEK);

  int *fd = syscall_num + 1;
  unsigned *position = (unsigned *) (syscall_num + 2);

  if (!is_valid_ptr ((void *) fd) ||
      !is_valid_ptr ((void *) position))
    {
      lock_release (&thread_filesys_lock);
      thread_exit ();
    }

  struct thread *curr = thread_current ();
  int i;
  for (i = 0; i < MAX_FILES; i++)
    {
      if (curr->open_files[i].used == 1 && curr->open_files[i].fd == *fd)
        {
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
tell (struct intr_frame *f)
{
  lock_acquire (&thread_filesys_lock);
  int *syscall_num = (int *) (f->esp);
  ASSERT (*syscall_num == SYS_TELL);

  int *fd = syscall_num + 1;
  if (!is_valid_ptr ((void *) fd))
    {
      lock_release (&thread_filesys_lock);
      thread_exit ();
    }

  struct thread *curr = thread_current ();
  int i;
  for (i = 0; i < MAX_FILES; i++)
    {
      if (curr->open_files[i].used == 1 && curr->open_files[i].fd == *fd)
        {
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
close (struct intr_frame *f)
{
  lock_acquire (&thread_filesys_lock);
  int *syscall_num = (int *) (f->esp);
  ASSERT (*syscall_num == SYS_CLOSE);

  int *fd = syscall_num + 1;
  if (!is_valid_ptr ((void *) fd))
    {
      lock_release (&thread_filesys_lock);
      thread_exit ();
    }

  struct thread *curr = thread_current ();
  int i;
  for (i = 0; i < MAX_FILES; i++)
    {
      if (curr->open_files[i].used == 1 && curr->open_files[i].fd == *fd)
        {
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
