#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "filesys/file.h"

void syscall_init (void);
void close_file(struct file *);


#endif /* userprog/syscall.h */
