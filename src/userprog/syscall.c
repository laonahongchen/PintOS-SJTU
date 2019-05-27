#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <threads/vaddr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "process.h"

static void syscall_handler (struct intr_frame *);

static void close_file(struct file* file);
static void sys_halt(struct intr_frame *f);
static void sys_exit(struct intr_frame *f, int status);
static void sys_exec(struct intr_frame *f, const char *cmd_line);
static void sys_wait(struct intr_frame *f, pid_t pid);
static void sys_create(struct intr_frame *f, const char *file);
static void sys_remove(struct intr_frame *f, const char *file);
static void sys_open(struct intr_frame *f, const char *file);
static void sys_filesize(struct intr_frame *f, int fd);
static void sys_read(struct intr_frame *f, int fd, void *buffer, unsigned size);
static void sys_write(struct intr_frame *f, int fd, const void *buffer, unsigned size);
static void sys_seek(struct intr_frame *f, int fd, unsigned position);
static void sys_tell(struct intr_frame *f, int fd);
static void sys_close(struct intr_frame *f, int fd);

bool syscall_check_user_string(const char *str);
bool syscall_check_user_buffer(const char *str, int size, bool write);

static struct lock filesys_lock;

void
syscall_init (void) 
{
  lock_init(*filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void
exit_status(struct intr_frame *f, int status) {
  struct thread *cur = thread_current();
  cur->return_value = status;
  f->eax = status;
  thread_exit();
}

bool
check_translate_user(const char *vaddr, bool write) {
  if(vaddr == NULL || !is_user_vaddr(vaddr))
    return false;
  return pagedir_get_page(thread_current() -> pagedir, vaddr) != NULL;
}

bool
check_user(const char *vaddr, int size, bool write) {
  if(!check_translate_user(vaddr + size - 1, write))
    return false;
  size >>= 12; // to page num so that we only need to check is every page valid
  do {
    if(!check_translate_user(vaddr, size, write))
      return false;
    vaddr += (1<<12);
  } while(size--);
  return true;
}



static void
syscall_handler (struct intr_frame *f /*UNUSED*/) 
{
  if(!check_user(f->esp, 4, false))
    exit_status(f, -1);
  int syscall_num = *((int*)f->esp);
  void *arg1 = f->esp + 4, *arg2 = f->esp + 8, *arg3 = f->esp + 12;

  switch (syscall_num) {
    case SYS_EXIT: case SYS_EXEC: case SYS_WAIT: case SYS_TELL: case SYS_CLOSE: case SYS_REMOVE: case SYS_OPEN: case SYS_FILESIZE:
      if(!check_user(arg1, 4, false))
        exit_status(f, -1);
      break;
    case SYS_SEEK: case SYS_CREATE:
      if(!check_user(arg1, 8, false))
        exit_status(f, -1);
      break;
    case SYS_OPEN: case SYS_CLOSE:
      if(!check_user(arg1, 12, false))
        exit_status(f, -1);
      break;
    default:;
  }
  switch (syscall_num) {
    case SYS_HALT:
      sys_halt(f); break;
    case SYS_EXIT:
      sys_exit(f, *((int *)arg1)); break;
    case SYS_EXEC:
      sys_exec(f, *((void **) arg1)); break;
    case SYS_WAIT:
      sys_wait(f, *((int *)arg1)); break;
    case SYS_CREATE:
      sys_create(f, *((void **) arg1), *((unsigned *) arg2)); break;
    case SYS_REMOVE:
      sys_remove(f, *((void **) arg1)); break;
    case SYS_OPEN:
      sys_open(f, *((void **) arg1)); break;
    case SYS_FILESIZE:
      sys_filesize(f, *((int *)arg1)); break;
    case SYS_READ:
      sys_read(f, *((int *)arg1, *((void **) arg2), *((unsigned *) arg3)); break;
    case SYS_WRITE:
      sys_write(f, *((int *)arg1, *((void **) arg2), *((unsigned *) arg3)); break;
    case SYS_SEEK:
      sys_seek(f, *((int *)arg1, *((unsigned *) arg2)); break;
    case SYS_TELL:
      sys_tell(f, *((int *)arg1)); break;
    case SYS_CLOSE:
      sys_close(f, *((int *)arg1)); break;

  }

}

static void
sys_halt(struct intr_frame *f) {
  shutdown_power_off();
}

static void
sys_wait(struct intr_frame *f, pid_t pid) {
  f->eax = (uint32_t)process_wait(pid);
}

static void
sys_exit(struct intr_frame *f, int status) {
  struct thread *cur = thread_current();
  if (!cur->parent_die) {
    cur->message_to_parent->exited = true;
    cur->message_to_parent->ret_value = status;
  }
  return status;
}

static void
sys_write(struct intr_frame *f, int fd, const void *buffer, unsigned size) {
  if (!check_user(buffer, size, false))
    exit_status(f, -1);
  if (fd == STDIN_FILENO)
    exit_status(f, -1);
  else if (fd == STDOUT_FILENO) {
    putbuf(buffer, size);
  } else {
    struct file_info *info = get_file_info(fd);
    if(info != NULL && !inode_isdir(file_get_inode(info->opened_file))) {
      lock_acquire(&filesys_lock);
      f->eax = file_write(info->opened_file, buffer, size);
      lock_release(&filesys_lock);
    } else {
      exit_status(f,-1);
    }
  }
}

static void
sys_read(struct intr_frame *f, int fd, const void *buffer, int size) {
  if(!check_user(buffer, size, true))
    exit_status(f, -1);
  if(fd == STDOUT_FILENO)
    exit_status(f, -1);
  uint8_t str = buffer;
  if(fd == STDIN_FILENO) {
    while(size--) {
      *(char *)str++ = input_getc();
    }
  } else {
    struct file_info *info = get_file_info(fd);
    if(t != NULL) {
      lock_acquire(&filesys_lock);
      f->eax = (uint32_t)file_read(info->opened_file, buffer, size);
      lock_release(&filesys_lock);
    } else {
      exit_status(f, -1);
    }
  }
}

static void
close_file(struct file* file) {
  lock_acquire(&filesys_lock);
  file_close(file);
  lock_release(&filesys_lock);
}

bool
check_string(const char *str) {
  if(!check_translate_user(str, false))
    return false;
  int cnt = 0;
  while(*(str + cnt) != 0) {
    if(cnt == 4095)
      return false;
    cnt++;
    if(((int)(str + cnt)) & PGMASK == 0) {
      if (!check_translate_user(str + cnt, false))
        return false;
    }
  }
  return true;
}

static void
sys_exec(struct intr_frame *f, const char *cmd_line) {
  if(!check_string(cmd_line)) {
    exit_status(f, -1);
  }
  lock_acquire(&filesys_lock);
  f->eax = (uint32_t)process_execute(cmd_line);
  lock_release(&filesys_lock);
  struct list_elem *e;
  struct thread *cur = thread_current();
  struct child_message *l;
  for(e = list_begin(&cur->child_list); e != list_end(&cur->child_list); e = list_next(e)) {
    l = list_entry(e, struct child_info, elem);
    if(l->child_id == f->eax) {
      sema_down(l->sema_start);
      if(l->load_failed)
        f->eax = (uint32_t)-1;
      return ;
    }
  }
}

static void
sys_open(struct intr_frame *f, const char *name) {
  if(!check_string(name))
    exit_status(f, -1);
  lock_acquire(&filesys_lock);
  file *tmp = (uint32_t)filesys_open(name);
  lock_release(&filesys_lock);
  if(tmp == NULL) {
    f->eax = (uint32_t)-1;
    return ;
  }
  static uint32_t fd_next = 2;
  struct file_info *info = malloc(sizeof(struct file_info));
  info->opened_file = tmp;
  info->thread_num = thread_current();
  info->fd = fd_next++;
  add_file_list(info);
  f->eax = (uint32_t)info->fd;
}

static void
sys_create(struct intr_frame *f, const char *name, unsigned initial_size) {
  if(!check_string(name))
    exit_status(f, -1);
  lock_acquire(&filesys_lock);
  f->eax = (uint32_t)filesys_create(name, initial_size);
  lock_release(&filesys_lock);
}

static void
sys_remove(struct intr_frame *f, const char *name) {
  if(!check_string(name))
    exit_status(f, -1);
  lock_acquire(&filesys_lock);
  f->eax = (uint32_t)filesys_remove(name);
  lock_release(&filesys_lock);
}

static void
sys_filesize(struct intr_frame *f, int fd) {
  struct file_info *info = get_file_info(fd);
  if(t != NULL) {
    lock_acquire(&filesys_lock);
    f->eax = (uint32_t)file_length(info->opened_file);
    lock_release(&filesys_lock);
  } else {
    exit_status(f, -1);
  }
}

static void
sys_close(struct intr_frame *f, int fd) {
  struct file_info *info = get_file_info(fd);
  if(t != NULL) {
    lock_acquire(&filesys_lock);
    file_close(info->opened_file);
    lock_release(&filesys_lock);
    list_remove(&info->elem);
    free(info);
  } else {
    exit_status(f, -1);
  }
}

static void
sys_tell(struct intr_frame *f, int fd) {
  struct file_info *info = get_file_info(fd);
  if(info != NULL) {//--------------------need change after filesys finished -------
    lock_acquire(&filesys_lock);
    f->eax = (uint32_t)file_tell(info->opened_file);
    lock_release(&filesys_lock);
  } else {
    exit_status(f, -1);
  }
}

static void
sys_seek(struct intr_frame *f, int fd, unsigned position) {
  struct file_info *info = get_file_info(fd);
  if(info != NULL) {
    lock_acquire(&filesys_lock);
    file_seek(info->opened_file, position);
    lock_release(&filesys_lock);
  } else {
    exit_status(f, -1);
  }
}

