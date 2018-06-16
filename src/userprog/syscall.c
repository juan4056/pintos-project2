#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
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

void syscall_init(void)
{
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
    exit(f);
    break;
  case SYS_WAIT:
    wait(f);
    break;
  case SYS_WRITE:
    write(f);
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
  arg_list[0] = usraddr_to_keraddr_ptr(arg_list[0]);

  pid_t pid = process_execute(cmd_line);
  struct child_process *cp = get_child_process(pid);
  ASSERT(cp);
  while (cp->load == 0)
  {
    // Not Loaded
    barrier();
  }
  if (cp->load == 2)
  {
    // Loaded failed
    pid = -1;
  }
  f->eax = pid;
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
  struct thread *current_thread = thread_current();
  // if (is_thread_alive(current_thread->parent))
  // {
  //   current_thread->cp->status = status;
  // }
  printf("%s: exit(%d)\n", current_thread->name, status);
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
  if (!is_user_vaddr(vaddr) || vaddr < ((void *)0x08048000))
  {
    exit_process(-1);
  }
}