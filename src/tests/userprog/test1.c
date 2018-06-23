#include <syscall.h>
#include "tests/lib.h"
#include "tests/userprog/sample.inc"
#include "tests/main.h"

void
test_main (void) 
{
    CHECK (create ("quux.dat", sizeof sample - 1), "create \"quux.dat\"");
    CHECK (remove ("quux.dat"), "remove quux.dat");
    CHECK (create ("quux.dat", sizeof sample - 1), "create \"quux.dat\"");
    CHECK (create ("quux.dat", sizeof sample - 1) == false, "Can not create \"quux.dat\" again");
}