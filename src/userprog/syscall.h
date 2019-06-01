#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "filesys/file.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <threads/vaddr.h>
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/thread.h"

void syscall_init (void);
void close_file(struct file *);
void exit_status(struct intr_frame *f, int status);
bool check_translate_user(const char *vaddr, bool write);


#endif /* userprog/syscall.h */
