#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"

/* Serializes all access to the file system, which is not
   thread-safe. */
extern struct lock filesys_lock;

void syscall_init (void);
void syscall_close_files (void);

#endif /* userprog/syscall.h */
