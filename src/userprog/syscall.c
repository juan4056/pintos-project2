#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

void syscall_init(void);
static void syscall_handler(struct intr_frame *);
void halt(void);
void sys_exit(struct intr_frame *f);
void sys_wait(struct intr_frame *f);
void sys_write(struct intr_frame *f);
void extract_argument(struct intr_frame *f, int *arg_list, int num_of_arguments);
void is_buffer_valid(void *buffer, unsigned size);
int usraddr_to_keraddr_ptr(const void *vaddr);
void check_valid_ptr(const void *vaddr);
void sys_exec(struct intr_frame *f);
struct lock filesys_lock;
void sys_create(struct intr_frame *f);
void sys_open(struct intr_frame *f);
void sys_close(struct intr_frame *f);
void sys_read(struct intr_frame *f);
void sys_file_size(struct intr_frame *f);
void sys_seek(struct intr_frame *f);
void sys_tell(struct intr_frame *f);
void sys_remove(struct intr_frame *f);

void syscall_init(void)
{
  lock_init(&filesys_lock);
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
  check_valid_ptr(f->esp);
  int option = *(int *)f->esp;
  switch (option)
  {
  case SYS_HALT:
    halt();
    break;
  case SYS_EXIT:
    sys_exit(f);
    break;
  case SYS_WAIT:
    sys_wait(f);
    break;
  case SYS_CREATE:
    sys_create(f);
    break;
  case SYS_OPEN:
    sys_open(f);
    break;
  case SYS_CLOSE:
    sys_close(f);
    break;
  case SYS_WRITE:
    sys_write(f);
    break;
  case SYS_READ:
    sys_read(f);
    break;
  case SYS_FILESIZE:
    sys_file_size(f);
    break;
  case SYS_SEEK:
    sys_seek(f);
    break;
  case SYS_TELL:
    sys_tell(f);
    break;
  case SYS_EXEC:
    sys_exec(f);
    break;
  case SYS_REMOVE:
    sys_remove(f);
    break;
  default:
    printf("system call!\n");
    exit_process(-1);
  }
}

void sys_exec(struct intr_frame *f)
{
  // Exit current thread
  int arg_list[3];
  extract_argument(f, arg_list, 1);
  arg_list[0] = usraddr_to_keraddr_ptr((const void *)arg_list[0]);
  const char *cmd_line = (const char *)arg_list[0];
  tid_t tid = process_execute(cmd_line);
  struct child_process_warpper *warpper = get_child_process_warpper(tid);
  ASSERT(warpper);
  while (warpper->load == 0)
  {
    // Not Loaded
    barrier();
  }
  if (warpper->load == -1)
  {
    // Loaded failed
    tid = -1;
  }
  f->eax = tid;
}

void halt(void)
{
  shutdown();
}

void sys_exit(struct intr_frame *f)
{
  // Exit current thread
  int arg_list[3];
  extract_argument(f, arg_list, 1);
  int new_status = arg_list[0];
  exit_process(new_status);
}

void exit_process(int status)
{
  struct thread *current_thread = thread_current();
  if (is_thread_alive(current_thread->parent->tid))
  {
    current_thread->warpper->status = status;
  }
  printf("%s: exit(%d)\n", current_thread->name, status);
  thread_exit();
}

void sys_wait(struct intr_frame *f)
{
  int arg_list[3];
  extract_argument(f, arg_list, 1);
  int tid = arg_list[0];
  f->eax = process_wait(tid);
}

void sys_create(struct intr_frame *f)
{
  int arg_list[3];
  extract_argument(f, arg_list, 2);
  arg_list[0] = usraddr_to_keraddr_ptr((const void *)arg_list[0]);
  lock_acquire(&filesys_lock);
  bool success = filesys_create((const char *)arg_list[0], (unsigned)arg_list[1]);
  lock_release(&filesys_lock);
  f->eax = success;
}

void sys_open(struct intr_frame *f)
{
  int arg_list[3];
  extract_argument(f, arg_list, 1);
  arg_list[0] = usraddr_to_keraddr_ptr((const void *)arg_list[0]);
  lock_acquire(&filesys_lock);
  const char *file_name = (const char *)arg_list[0];
  struct file *file_ptr = filesys_open(file_name);
  if (!file_ptr)
  {
    lock_release(&filesys_lock);
    f->eax = -1;
    return;
  }
  int fd = add_file_to_process(file_ptr);
  lock_release(&filesys_lock);
  f->eax = fd;
}

void sys_close(struct intr_frame *f)
{
  int arg_list[3];
  extract_argument(f, arg_list, 1);
  int fd = arg_list[0];
  lock_acquire(&filesys_lock);
  close_process_file(fd);
  lock_release(&filesys_lock);
}

void sys_write(struct intr_frame *f)
{
  int arg_list[3];
  extract_argument(f, arg_list, 3);
  int fd = arg_list[0];
  is_buffer_valid((void *)arg_list[1], (unsigned)arg_list[2]);
  arg_list[1] = usraddr_to_keraddr_ptr((const void *)arg_list[1]);
  void *buffer = (void *)arg_list[1];
  unsigned size = arg_list[2];
  //printf("----ptr checked!\n");
  if (fd == STDOUT_FILENO)
  {
    //printf("----std out!\n");
    putbuf(buffer, size);
    f->eax = size;
    return;
  }
  lock_acquire(&filesys_lock);
  struct file_warpper *warpper = get_file_warpper(fd);
  struct file *file_ptr = warpper->file;
  if (!file_ptr)
  {
    lock_release(&filesys_lock);
    f->eax = -1;
    return;
  }
  if(warpper->exec_file == 1){
    // file_deny_write(file_ptr);
    lock_release(&filesys_lock);
    f->eax = 0;
    return;
  }
  off_t bytes = file_write(file_ptr, buffer, size);
  lock_release(&filesys_lock);
  f->eax = bytes;
}

void sys_read(struct intr_frame *f)
{
  int arg_list[3];
  extract_argument(f, arg_list, 3);
  int fd = arg_list[0];
  void *buffer = (void *)arg_list[1];
  unsigned size = arg_list[2];
  is_buffer_valid(buffer, size);
  if (fd == STDIN_FILENO)
  {
    unsigned i;
    uint8_t *local_buffer = (uint8_t *)buffer;
    for (i = 0; i < size; i++)
    {
      local_buffer[i] = input_getc();
    }
    f->eax = size;
    return;
  }
  lock_acquire(&filesys_lock);
  struct file_warpper *warpper = get_file_warpper(fd);
  struct file *file_ptr = warpper->file;
  if (!file_ptr)
  {
    lock_release(&filesys_lock);
    f->eax = -1;
    return;
  }
  int bytes = file_read(file_ptr, buffer, size);
  lock_release(&filesys_lock);
  f->eax = bytes;
}

void sys_file_size(struct intr_frame *f)
{
  int arg_list[3];
  extract_argument(f, arg_list, 1);
  int fd = arg_list[0];
  lock_acquire(&filesys_lock);
  struct file_warpper *warpper = get_file_warpper(fd);
  struct file *file_ptr = warpper->file;
  if (!file_ptr)
  {
    lock_release(&filesys_lock);
    f->eax = -1;
    return;
  }
  int size = file_length(file_ptr);
  lock_release(&filesys_lock);
  f->eax = size;
}

void sys_seek(struct intr_frame *f)
{
  int arg_list[3];
  extract_argument(f, arg_list, 2);
  int fd = arg_list[0];
  unsigned posi = arg_list[1];
  lock_acquire(&filesys_lock);
  struct file_warpper *warpper = get_file_warpper(fd);
  struct file *file_ptr = warpper->file;
  if (!file_ptr)
  {
    lock_release(&filesys_lock);
    return;
  }
  file_seek(file_ptr, posi);
  lock_release(&filesys_lock);
}

void sys_tell(struct intr_frame *f)
{
  int arg_list[3];
  extract_argument(f, arg_list, 1);
  int fd = arg_list[0];
  lock_acquire(&filesys_lock);
  struct file_warpper *warpper = get_file_warpper(fd);
  struct file *file_ptr = warpper->file;
  if (!file_ptr)
  {
    lock_release(&filesys_lock);
    f->eax = -1;
  }
  off_t offset = file_tell(file_ptr);
  lock_release(&filesys_lock);
  f->eax = offset;
}

void sys_remove(struct intr_frame *f)
{
  int arg_list[3];
  extract_argument(f, arg_list, 1);
  arg_list[0] = usraddr_to_keraddr_ptr((const void *)arg_list[0]);
  const char *file_name = (const char *)arg_list[0];
  lock_acquire(&filesys_lock);
  bool success = filesys_remove(file_name);
  lock_release(&filesys_lock);
  f->eax = success;
}

void extract_argument(struct intr_frame *f, int *arg_list, int num_of_arguments)
{
  int i;
  int *ptr;
  for (i = 0; i < num_of_arguments; i++)
  {
    ptr = (int *)f->esp + i + 1;
    check_valid_ptr(ptr);
    arg_list[i] = *ptr;
  }
}

void is_buffer_valid(void *buffer, unsigned size)
{
  unsigned i;
  char *local_buffer = (char *)buffer;
  for (i = 0; i < size; i++)
  {
    check_valid_ptr(local_buffer);
    local_buffer++;
  }
}

int usraddr_to_keraddr_ptr(const void *vaddr)
{
  // TO DO: Need to check if all bytes within range are correct
  // for strings + buffers
  check_valid_ptr(vaddr);
  void *ptr = pagedir_get_page(thread_current()->pagedir, vaddr);
  if (!ptr)
  {
    exit_process(-1);
  }
  return (int)ptr;
}

void check_valid_ptr(const void *vaddr)
{
  if ((is_user_vaddr(vaddr) == false) || (vaddr < ((void *)0x08048000)))
  {
    exit_process(-1);
  }
}