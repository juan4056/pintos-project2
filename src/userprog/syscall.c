#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"

void syscall_init(void);
static void syscall_handler(struct intr_frame *);
void halt(void);
void exit(struct intr_frame *f);
int wait(struct intr_frame *f);
int write(struct intr_frame *f);
void extract_argument(struct intr_frame *f, int *arg_list, int num_of_arguments);
void is_buffer_valid(void *buffer, unsigned size);
int usraddr_to_keraddr_ptr(const void *vaddr);
void check_valid_ptr(const void *vaddr);
void exit_process(int status);
void exec(struct intr_frame *f);

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
  printf("-----System Call!\n");
  check_valid_ptr(f->esp);
  printf("-----Pass Check\n");
  int option = *(int *)f->esp;
  switch (option)
  {
  case SYS_HALT:
    halt();
    break;
  case SYS_EXIT:
    exit(f);
    break;
  case SYS_WAIT:
    wait(f);
    break;
  case SYS_WRITE:
    write(f);
    break;
  case SYS_EXEC:
    exec(f);
    break;
  default:
    printf("system call!\n");
    exit_process(-1);
  }
}

void exec(struct intr_frame *f)
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

void exit(struct intr_frame *f)
{
  // Exit current thread
  int arg_list[3];
  extract_argument(f, arg_list, 1);
  int new_status = arg_list[0];
  exit_process(new_status);
}

void exit_process(int status)
{
  if(status == -1){
    printf("-----Kill by system!\n");
  }
  struct thread *current_thread = thread_current();
  if (is_thread_alive(current_thread->parent->tid))
  {
    current_thread->warpper->status = status;
  }
  printf("%s: exit(%d)\n", current_thread->name, status);
  thread_exit();
}

int wait(struct intr_frame *f)
{
  int arg_list[3];
  extract_argument(f, arg_list, 1);
  int tid = arg_list[0];
  f->eax = process_wait(tid);
  return 1;
}

int write(struct intr_frame *f)
{
  int arg_list[3];
  extract_argument(f, arg_list, 3);
  int fd = arg_list[0];
  void *buffer = (void *)arg_list[1];
  unsigned size = arg_list[2];

  is_buffer_valid(buffer, size);
  // arg[1] = usraddr_to_keraddr_ptr((const void *)arg[1]);

  f->eax = process_write(fd, buffer, size);
  return 0;
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