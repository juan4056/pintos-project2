# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(test2) begin
(test2) create "quux.dat"
(test2) open "quux.dat"
(test2) remove quux.dat
(test2) filesize = 239
(test2) end
test2: exit(0)
EOF
pass;
