#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/synch.h"
#include "threads/thread.h"

/* Exit information about a child process, shared between the
   child and its parent.  It lives until both have dropped their
   reference, so the parent can still wait() after the child is
   gone and the child can exit after the parent is gone. */
struct child_status
  {
    tid_t tid;                          /* Child's thread id. */
    int exit_status;                    /* Valid once the child has exited. */
    struct semaphore exited;            /* Upped once when the child exits. */
    int ref_cnt;                        /* Parent + child references. */
    struct list_elem elem;              /* Element in parent's children list. */
  };

void process_init (void);
tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
