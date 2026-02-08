x86-64 Windows analysis program for life-like Cellular Automata on an 8x8 torus.

The project build requirements are listed at the top of the makefile

Almost all configuration macros in life.c are passed directly from the makefile, with the exception of CLIP maps to CLIPBOARD, RULESET maps to both RULESET and NEXT_COND, ISA adds extra information in native builds, RAND_BUF_LEN=1 implies -mrdrnd for non-native builds, and NEIGHBORHOOD prepends `NH_` to the start of the value.

Valid neighborhoods are MOORE (all 8), VON_NEUMANN (cardinal only), and DIAGONAL.

Rulesets are something like `B<nums>/S<nums>`. The B numbers are the neighbors counts for a dead cell to become alive and the S numbers are the neighbor counts for a living cell to continue living. For example, the default ruleset of B3/S23 means dead cells with 3 neighbors become alive and alive cells with 2 or 3 neighbors continue living. Since the counters are only 3 bits, the neighbor counts are the same modulo 8, so neither of the counts can have 8 without 0 or 0 without 8.

<!-- intersting, ruleset B012568/S03478 has the longest condition, character wise -->
<!-- also, 0x85080b001d0004b4 has 15240 predecessors -->

for help on individual programs, use the `-h` flag. Except for `life-flaunch.exe` which doesn't take any arguments and always runs `life -Hf nrun inf`, and `life-blaunch.exe` which doesn't take any arguments and always runs `life -Hf burn inf`.
