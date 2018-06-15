#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "userprog/process.h"

void syscall_init(void);
static void syscall_handler(struct intr_frame *);
void halt(void);
void exit(struct intr_frame *f);
int wait(struct intr_frame *f);
void extract_argument(struct intr_frame *f, int *arg_list, int num_of_arguments);

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
  printf("system call!\n");
  is_user_vaddr(f->esp);
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
  default:
    printf("system call!\n");
    thread_exit();
  }
}

void halt(void)
{
  shutdown();
}

void exit(struct intr_frame *f)
{
  // Exit current thread
  printf("------------Exit\n");
  int arg_list[3];
  extract_argument(f, arg_list, 1);
  int new_status = arg_list[0];
  struct thread *current_thread = thread_current();
  // if (thread_alive(current_thread->parent))
  // {
  //   current_thread->cp->status = new_status;
  // }
  thread_exit();
}

int wait(struct intr_frame *f)
{
  int arg_list[3];
  extract_argument(f, arg_list, 1);
  int pid = arg_list[0];
  f->eax = process_wait(pid);
  return 1;
}

int write(struct intr_frame *f)
{
  int arg_list[3];
  extract_argument(f, arg_list, 3);
  int fd = arg_list[0];
  void *buffer = arg_list[1];
  unsigned size = arg_list[2];

  // check_valid_buffer((void *)arg[1], (unsigned)arg[2]);
  // arg[1] = user_to_kernel_ptr((const void *)arg[1]);

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
    is_user_vaddr(ptr);
    arg_list[i] = *ptr;
  }
}