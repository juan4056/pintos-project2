# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_USER_FAULTS => 1, [<<'EOF']);
(bad-read3) begin
bad-read3: exit(-1)
EOF
pass;