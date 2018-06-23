/* Creates an ordinary empty file. */
// And remove it

#include <syscall.h>
#include "tests/userprog/sample.inc"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  int handle, byte_cnt;
  CHECK (create ("quux.dat", sizeof sample - 1), "create \"quux.dat\"");
  CHECK ((handle = open ("quux.dat")) > 1, "open \"quux.dat\"");
  CHECK (remove ("quux.dat"), "remove quux.dat");
  byte_cnt = write (handle, sample, sizeof sample - 1);
  if (byte_cnt != sizeof sample - 1)
    fail ("write() returned %d instead of %zu", byte_cnt, sizeof sample - 1);
  msg ("filesize = %d", filesize(handle));
}