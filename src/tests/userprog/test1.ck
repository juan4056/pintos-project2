# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_USER_FAULTS => 1, [<<'EOF']);
(test1) begin
(test1) create "quux.dat"
(test1) remove quux.dat
(test1) create "quux.dat"
(test1) Can not create "quux.dat" again
(test1) end
test1: exit(0)
EOF
pass;