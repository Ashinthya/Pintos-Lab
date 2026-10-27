#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

/* An open file, as seen through a file descriptor. */
struct file_desc
  {
    int fd;                             /* Descriptor number, >= 2. */
    struct file *file;                  /* The open file. */
    struct list_elem elem;              /* Element in thread's fd_list. */
  };

struct lock filesys_lock;

static void syscall_handler (struct intr_frame *);

void
syscall_init (void)
{
  lock_init (&filesys_lock);
  process_init ();
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Terminates the current process with STATUS. */
static void
sys_exit (int status)
{
  thread_current ()->exit_status = status;
  thread_exit ();
}

/* User memory access.  Every user pointer is checked before the
   kernel touches it: it must be below PHYS_BASE and mapped in the
   process's page directory.  A bad pointer kills the process with
   exit status -1.  Checks happen before any lock is taken or any
   memory is allocated, so there is nothing to clean up on
   failure. */

/* Kills the process unless UADDR is a mapped user address that is
   also writable if WRITABLE is true. */
static void
check_user_access (const void *uaddr, bool writable)
{
  uint32_t *pd = thread_current ()->pagedir;

  if (uaddr == NULL || !is_user_vaddr (uaddr)
      || pagedir_get_page (pd, uaddr) == NULL
      || (writable && !pagedir_is_writable (pd, uaddr)))
    sys_exit (-1);
}

/* Kills the process unless UADDR is a mapped user address. */
static void
check_user_addr (const void *uaddr)
{
  check_user_access (uaddr, false);
}

/* Kills the process unless all of [UADDR, UADDR + SIZE) is mapped
   user memory, writable too if WRITABLE is true.  Checks the first
   byte, then one address in each following page, then the last
   byte. */
static void
check_user_range (const void *uaddr, size_t size, bool writable)
{
  uintptr_t start = (uintptr_t) uaddr;
  uintptr_t end = start + size - 1;
  uintptr_t page;

  if (size == 0)
    return;
  if (end < start)
    sys_exit (-1);
  check_user_access ((const void *) start, writable);
  for (page = (uintptr_t) pg_round_down (uaddr) + PGSIZE; page <= end;
       page += PGSIZE)
    check_user_access ((const void *) page, writable);
  check_user_access ((const void *) end, writable);
}

/* Kills the process unless all of [UADDR, UADDR + SIZE) is mapped
   user memory. */
static void
check_user_buffer (const void *uaddr, size_t size)
{
  check_user_range (uaddr, size, false);
}

/* Like check_user_buffer(), but the kernel will write to the
   buffer, so it must also be writable.  Otherwise writing to a
   read-only page (e.g. the code segment) would fault in the
   kernel and panic. */
static void
check_user_writable (void *uaddr, size_t size)
{
  check_user_range (uaddr, size, true);
}

/* Kills the process unless STR is a null-terminated string in
   mapped user memory.  Re-checks only when crossing into a new
   page. */
static void
check_user_string (const char *str)
{
  check_user_addr (str);
  while (*str != '\0')
    {
      str++;
      if (pg_ofs (str) == 0)
        check_user_addr (str);
    }
}

/* Returns system call argument N (0 is the call number) from the
   user stack at F->esp. */
static uint32_t
get_arg (struct intr_frame *f, int n)
{
  uint32_t *arg = (uint32_t *) f->esp + n;
  check_user_buffer (arg, sizeof *arg);
  return *arg;
}

/* File descriptors. */

/* Returns the current process's open file for FD, or NULL. */
static struct file_desc *
lookup_fd (int fd)
{
  struct list *fds = &thread_current ()->fd_list;
  struct list_elem *e;

  for (e = list_begin (fds); e != list_end (fds); e = list_next (e))
    {
      struct file_desc *desc = list_entry (e, struct file_desc, elem);
      if (desc->fd == fd)
        return desc;
    }
  return NULL;
}

/* Closes all of the current process's open files. */
void
syscall_close_files (void)
{
  struct list *fds = &thread_current ()->fd_list;

  while (!list_empty (fds))
    {
      struct file_desc *desc = list_entry (list_pop_front (fds),
                                           struct file_desc, elem);
      lock_acquire (&filesys_lock);
      file_close (desc->file);
      lock_release (&filesys_lock);
      free (desc);
    }
}

/* System call implementations. */

static int
sys_exec (const char *cmd_line)
{
  check_user_string (cmd_line);
  return process_execute (cmd_line);
}

static bool
sys_create (const char *file, unsigned initial_size)
{
  bool success;

  check_user_string (file);
  lock_acquire (&filesys_lock);
  success = filesys_create (file, initial_size);
  lock_release (&filesys_lock);
  return success;
}

static bool
sys_remove (const char *file)
{
  bool success;

  check_user_string (file);
  lock_acquire (&filesys_lock);
  success = filesys_remove (file);
  lock_release (&filesys_lock);
  return success;
}

static int
sys_open (const char *file_name)
{
  struct thread *cur = thread_current ();
  struct file_desc *desc;
  struct file *file;

  check_user_string (file_name);
  lock_acquire (&filesys_lock);
  file = filesys_open (file_name);
  lock_release (&filesys_lock);
  if (file == NULL)
    return -1;

  desc = malloc (sizeof *desc);
  if (desc == NULL)
    {
      lock_acquire (&filesys_lock);
      file_close (file);
      lock_release (&filesys_lock);
      return -1;
    }
  desc->fd = cur->next_fd++;
  desc->file = file;
  list_push_back (&cur->fd_list, &desc->elem);
  return desc->fd;
}

static int
sys_filesize (int fd)
{
  struct file_desc *desc = lookup_fd (fd);
  int size;

  if (desc == NULL)
    return -1;
  lock_acquire (&filesys_lock);
  size = file_length (desc->file);
  lock_release (&filesys_lock);
  return size;
}

static int
sys_read (int fd, void *buffer, unsigned size)
{
  struct file_desc *desc;
  int bytes;

  check_user_writable (buffer, size);
  if (fd == STDIN_FILENO)
    {
      uint8_t *buf = buffer;
      unsigned i;

      for (i = 0; i < size; i++)
        buf[i] = input_getc ();
      return size;
    }

  desc = lookup_fd (fd);
  if (desc == NULL)
    return -1;
  lock_acquire (&filesys_lock);
  bytes = file_read (desc->file, buffer, size);
  lock_release (&filesys_lock);
  return bytes;
}

static int
sys_write (int fd, const void *buffer, unsigned size)
{
  struct file_desc *desc;
  int bytes;

  check_user_buffer (buffer, size);
  if (fd == STDOUT_FILENO)
    {
      putbuf (buffer, size);
      return size;
    }

  desc = lookup_fd (fd);
  if (desc == NULL)
    return -1;
  lock_acquire (&filesys_lock);
  bytes = file_write (desc->file, buffer, size);
  lock_release (&filesys_lock);
  return bytes;
}

static void
sys_seek (int fd, unsigned position)
{
  struct file_desc *desc = lookup_fd (fd);

  if (desc == NULL)
    return;
  lock_acquire (&filesys_lock);
  file_seek (desc->file, position);
  lock_release (&filesys_lock);
}

static unsigned
sys_tell (int fd)
{
  struct file_desc *desc = lookup_fd (fd);
  unsigned position;

  if (desc == NULL)
    return -1;
  lock_acquire (&filesys_lock);
  position = file_tell (desc->file);
  lock_release (&filesys_lock);
  return position;
}

static void
sys_close (int fd)
{
  struct file_desc *desc = lookup_fd (fd);

  if (desc == NULL)
    return;
  list_remove (&desc->elem);
  lock_acquire (&filesys_lock);
  file_close (desc->file);
  lock_release (&filesys_lock);
  free (desc);
}

/* Reads the call number and arguments from the user stack,
   dispatches, and stores any return value in EAX. */
static void
syscall_handler (struct intr_frame *f)
{
  switch (get_arg (f, 0))
    {
    case SYS_HALT:
      shutdown_power_off ();
    case SYS_EXIT:
      sys_exit ((int) get_arg (f, 1));
    case SYS_EXEC:
      f->eax = sys_exec ((const char *) get_arg (f, 1));
      break;
    case SYS_WAIT:
      f->eax = process_wait ((tid_t) get_arg (f, 1));
      break;
    case SYS_CREATE:
      f->eax = sys_create ((const char *) get_arg (f, 1),
                           (unsigned) get_arg (f, 2));
      break;
    case SYS_REMOVE:
      f->eax = sys_remove ((const char *) get_arg (f, 1));
      break;
    case SYS_OPEN:
      f->eax = sys_open ((const char *) get_arg (f, 1));
      break;
    case SYS_FILESIZE:
      f->eax = sys_filesize ((int) get_arg (f, 1));
      break;
    case SYS_READ:
      f->eax = sys_read ((int) get_arg (f, 1), (void *) get_arg (f, 2),
                         (unsigned) get_arg (f, 3));
      break;
    case SYS_WRITE:
      f->eax = sys_write ((int) get_arg (f, 1), (const void *) get_arg (f, 2),
                          (unsigned) get_arg (f, 3));
      break;
    case SYS_SEEK:
      sys_seek ((int) get_arg (f, 1), (unsigned) get_arg (f, 2));
      break;
    case SYS_TELL:
      f->eax = sys_tell ((int) get_arg (f, 1));
      break;
    case SYS_CLOSE:
      sys_close ((int) get_arg (f, 1));
      break;
    default:
      sys_exit (-1);
    }
}
