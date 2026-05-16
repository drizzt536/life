# the following things are required:
	# basic linux tools: mv, rm, sed, awk, stat, grep, tail, touch
	# other linux tools: gcc (>=13, must be MSVCRT), make
	# binutils: ld, strip, objcopy, objdump
	# VC build tools: dumpbin, editbin (optional)
	# misc: 7z, wmic, nasm, python (>=3.12)

# this works for sure with MinGW devkit 2.5-2.8 (GCC 15.2-16.1, binutils 2.45-2.46)
# the MSYS2 version of GCC won't work because it is UCRT and not MSVCRT.

VERSION := 3.1.0

CFLAGS      := -Werror -Wall -Wextra -Wno-parentheses -Wno-missing-profile -std=gnu23 \
			-Iinclude -masm=intel -DPY_BASE=\"analyze\" -DVERSION=\"$(VERSION)\"
COPTZ       := -Ofast -fdelete-dead-exceptions -ffinite-loops -fgcse-las -fgcse-sm \
			-fipa-pta -fira-loop-pressure -flive-range-shrinkage -frename-registers \
			-fshort-enums -ftree-loop-if-convert -ftree-vectorize -fvpt -fweb -fwrapv \
			-fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-ident
CPROF_OPTZ  := -fprofile-partial-training -fbranch-probabilities -fipa-profile \
			-fprofile-reorder-functions
PROF_CFLAGS := -fprofile-generate -DPROFILING=true
LDFLAGS     := -s -O --as-needed --gc-sections --relax --exclude-all-symbols \
			--no-export-dynamic --disable-long-section-names --no-seh \
			--nxcompat --dynamicbase --high-entropy-va \
			--fatal-warnings --warn-common --warn-constructors
LDLIBS      := -lcryptbase -lkernel32 -lshell32 -lucrtbase -luser32
CFILES      := life.c $(wildcard ./include/*.h)

ifeq ($(PROFILE),false)
	CFLAGS += -DPGO=\"disabled\"
else
	CFLAGS += -DPGO=\"enabled\"
endif

CFLAGS_LIFE_O = -c -fprofile-use -nostdlib -ffreestanding $(CFLAGS) $(COPTZ) $(CPROF_OPTZ)

TRUTH_TABLE_CMD := $$(python ./gen-ruleset.py -tt2 '$(RULESET)' | awk 'BEGIN {ORS = " "} NR==1 {print "-DB_TT=" $$0 "u"} NR==2 {print "-DS_TT=" $$0 "u"; exit}')

# configuration stuff:

ifdef NO_VC
	override RESERVE := long
endif

# architecture to optimize for
ifeq ($(ISA),native)
	COPTZ += -march=native
	# CFLAGS is updated for native later
else
	CFLAGS += -DISA="\"$(ISA)\""

	ifeq ($(ISA),adx)
		COPTZ += -march=x86-64-v2 -mbmi -mbmi2 -mlzcnt -mmovbe -madx
		LDFLAGS += radix-sort.o
		PROF_CFLAGS += radix-sort.o
	else ifeq ($(ISA),avx512)
		COPTZ += -march=x86-64-v4 -madx
		LDFLAGS += radix-sort.o
		PROF_CFLAGS += radix-sort.o
	else ifeq ($(ISA),avx2)
		COPTZ += -march=x86-64-v3
		INCLUDE_RADIX := false
	else
		override ISA := popcnt
		# default to a super old one (POPCNT + SSE4.2)
		COPTZ += -march=x86-64-v2
		INCLUDE_RADIX := false
	endif
endif # native

ifdef NEIGHBORHOOD
	CFLAGS += -DNEIGHBORHOOD=NH_$(NEIGHBORHOOD)
endif

ifdef RULESET
	CFLAGS += -DRULESET=\"$(RULESET)\"
	CFLAGS += -DNEXT_COND="$$(cat ruleset.tmp)"
endif

ifdef ALIVE_CHAR_DEF
	CFLAGS += -DALIVE_CHAR_DEF=$(ALIVE_CHAR_DEF)
endif

ifdef DEAD_CHAR_DEF
	CFLAGS += -DDEAD_CHAR_DEF=$(DEAD_CHAR_DEF)
endif

ifdef TIMER_PERIOD
	CFLAGS += -DTIMER_PERIOD=$(TIMER_PERIOD)
endif

ifdef STEP_MOD_THRESH
	CFLAGS += -DSTEP_MOD_THRESH=$(STEP_MOD_THRESH)
endif

ifdef TABLE_BITS
	CFLAGS += -DTABLE_BITS=$(TABLE_BITS)
endif

ifdef PERIOD_LEN
	CFLAGS += -DPERIOD_LEN=$(PERIOD_LEN)
endif

ifdef TRANSIENT_LEN
	CFLAGS += -DTRANSIENT_LEN=$(TRANSIENT_LEN)
endif

ifdef RAND_BUF_LEN
	CFLAGS += -DRAND_BUF_LEN=$(RAND_BUF_LEN)

	ifeq ($(RAND_BUF_LEN),1)
		ifneq ($(ISA),native)
			# the unbuffered version uses RDRAND, and none of the `-march`
			# options come with RDRAND. Also, If ISA=native is given and
			# the machine doesn't have RDRAND, that should be an error.
			# otherwise, just assume RDRAND exists on the target machine.
			CFLAGS += -mrdrnd
		endif
	endif
endif

ifdef ARENA_LEN
	CFLAGS += -DARENA_LEN=$(ARENA_LEN)
endif

ifdef CLIP
	CFLAGS += -DCLIPBOARD=$(CLIP)
endif

ifdef HELP
	CFLAGS += -DHELP=$(HELP)
endif

ifdef BWSEARCH
	CFLAGS += -DBWSEARCH=$(BWSEARCH)
endif

ifdef WRAPPER
	CFLAGS += -DWRAPPER=$(WRAPPER)
endif

ifdef DEBUG
	CFLAGS += -DDEBUG=$(DEBUG)
endif

ifdef SHELL32
	ifeq ($(PROFILE),false)
		CFLAGS += -DSHELL32=$(SHELL32)
	else
		CFLAGS_LIFE_O += -DSHELL32=$(SHELL32)
	endif
endif

ifeq ($(PROFILE),false)
	ZIPFILE := life-v$(VERSION)-$(ISA).noprofile.7z
else
	ZIPFILE := life-v$(VERSION)-$(ISA).7z
endif

all: requirements $(ZIPFILE) bench

.PHONY: requirements req-7z req-nasm req-linux req-binutils req-gcc req-vcbtools req-python bench

requirements: req-7z req-nasm req-vcbtools req-python req-gcc

ifeq ($(REQUIRE),false)
req-7z:
req-nasm:
req-gcc:
req-linux:
req-binutils:
req-vcbtools:
req-python:
else
req-7z:
	@if ! command -v 7z > /dev/null; then echo "# program not found: \`7z\`"; exit 1; fi;
	# 7zip found

req-nasm:
	@if ! command -v nasm > /dev/null; then echo "# program not found: \`nasm\`"; exit 1; fi;
	# nasm found

req-python:
	@if ! command -v python > /dev/null; then  \
		echo "# program not found: \`python\`"; \
		exit 1; \
	fi;
	# python found

req-gcc: req-linux req-binutils
	@if ! command -v gcc > /dev/null; then echo "# program not found: \`gcc\`";  exit 1; fi; \
	if ! command -v grep > /dev/null; then echo "# program not found: \`grep\`"; exit 1; fi

ifneq ($(SKIP_GCC_CHECK),true)
	@# Sometimes the fuckass antivirus things the empty program is malware
	@# if you know it is MSVCRT, then you can just skip the check.
	echo "void main() {}" | gcc -x c -Wl,-s,--gc-sections - -o tmp.exe
	@if ! grep -Fq msvcrt.dll tmp.exe; then     \
		echo "rm -f tmp.exe";                   \
		rm -f tmp.exe;                          \
		echo "# GCC must be an MSVCRT version"; \
		exit 2;                                 \
	fi
	rm -f tmp.exe
endif
	# suitable version of GCC found

req-linux:
	@# basic linux utilities \
	if ! command -v mv    >/dev/null; then echo "# program not found: \`mv\`";   exit 1; fi; \
	if ! command -v rm    >/dev/null; then echo "# program not found: \`rm\`";   exit 1; fi; \
	if ! command -v awk   >/dev/null; then echo "# program not found: \`awk\`";  exit 1; fi; \
	if ! command -v sed   >/dev/null; then echo "# program not found: \`sed\`";  exit 1; fi; \
	if ! command -v stat  >/dev/null; then echo "# program not found: \`stat\`"; exit 1; fi; \
	if ! command -v touch >/dev/null; then echo "# program not found: \`touch\`";exit 1; fi;
ifneq ($(BENCH),false)
	@if ! command -v time >/dev/null; then echo "# program not found: \`time\`"; exit 1; fi;
endif
	# linux utilities found

req-binutils:
	@\
	if ! command -v ld      >/dev/null;then echo "# program not found: \`ld\`";      exit 1;fi; \
	if ! command -v strip   >/dev/null;then echo "# program not found: \`strip\`";   exit 1;fi; \
	if ! command -v objcopy >/dev/null;then echo "# program not found: \`objcopy\`"; exit 1;fi; \
	if ! command -v objdump >/dev/null;then echo "# program not found: \`objdump\`"; exit 1;fi;
	# binutils found

req-vcbtools:
ifndef NO_VC
	@\
	if ! command -v editbin >/dev/null;then echo "# program not found: \`editbin\`"; exit 1;fi; \
	if ! command -v dumpbin >/dev/null;then echo "# program not found: \`dumpbin\`"; exit 1;fi;
	# VC Build Tools found
endif # no vc
endif # require

bench: life.exe req-linux
	@# run 10 million trials. this takes around 3 seconds on my machine
ifeq ($(BENCH),false)
	# skipping benchmarking
else
	# benchmarking (5 trials)
	time -f %es ./life -Hqn 10000000 frun > /dev/null
	time -f %es ./life -Hqn 10000000 frun > /dev/null
	time -f %es ./life -Hqn 10000000 frun > /dev/null
	time -f %es ./life -Hqn 10000000 frun > /dev/null
	time -f %es ./life -Hqn 10000000 frun > /dev/null
endif

$(ZIPFILE): life.exe life-flaunch.exe life-blaunch.exe req-7z analyze.py req-linux
	7z a -t7z -mx=9 -bso0 -bsp0 $@ life.exe life-flaunch.exe life-blaunch.exe analyze.py

	@z=$$(stat -c %s $(ZIPFILE));   \
	a=$$(stat -c %s analyze.py);     \
	b=$$(stat -c %s life.exe);        \
	c=$$(stat -c %s life-flaunch.exe); \
	d=$$(stat -c %s life-blaunch.exe);  \
	awk "BEGIN {print \"# 7zip reduction: \" 100 - $$z*100 / ($$a + $$b + $$c + $$d) \"%\"}"

ruleset.tmp: gen-ruleset.py req-python
ifdef RULESET
	python $< '$(RULESET)' > $@
else
ruleset.tmp: req-linux req-python
	@# the content doesn't matter, just the file has to exist.
	touch $@
endif

init-crt.o: init-crt.nasm req-nasm req-binutils
	nasm -fwin64 $< -o $@
	@# only strip debug information. `-s` deletes everything.
	strip -S $@

radix-sort.o: radix-sort.nasm req-nasm req-binutils
	nasm -fwin64 $< -o $@
	strip -S $@

ifeq ($(PROFILE),false)

life.o: $(CFILES) req-gcc ruleset.tmp
	@# can't use `-ffreestanding` for some reason
ifeq ($(ISA),native)
	@# this is only in this branch. the non-profiling branch can have the regular name
	truth_table=$(TRUTH_TABLE_CMD); \
	flags=$$(gcc -march=native -Q --help=target 2>/dev/null | awk '/enabled/ {print $$1}'); \
	isa=$$(for f in AVX512 ADX BMI2 AVX2 AVX SSE4.2 SSE4.1 SSSE3 SSE3 SSE2 POPCNT; do echo "$$flags" | grep -iq $$f && { echo $$f; break; }; done); \
	cpu=$$(wmic cpu get name | sed -n 2p | awk '{$$1=$$1; print}'); \
	gcc -DISA="\"native (ISA='$$isa', CPU='$$cpu')\"" -c -nostdlib $(CFLAGS) $$truth_table $(COPTZ) $< -o $@.tmp
else
	truth_table=$(TRUTH_TABLE_CMD); \
	gcc -c -nostdlib $(CFLAGS) $$truth_table $(COPTZ) $< -o $@.tmp
endif # ISA

	objcopy $@.tmp --remove-section .pdata --remove-section .xdata $@
	strip -S $@
else # PROFILE
prof.exe: init-crt.o radix-sort.o $(CFILES) ruleset.tmp req-gcc
ifeq ($(ISA),native)
	truth_table=$(TRUTH_TABLE_CMD); \
	if gcc -Q --help=target -march=native | grep -F adx | grep -Fq enabled; then          \
		gcc $(PROF_CFLAGS) $(CFLAGS) $$truth_table $(COPTZ) $< radix-sort.o life.c -o $@; \
	else                                                                                  \
		gcc $(PROF_CFLAGS) $(CFLAGS) $$truth_table $(COPTZ) $< life.c -o $@;              \
	fi;
else
	truth_table=$(TRUTH_TABLE_CMD); \
	gcc $(PROF_CFLAGS) $(CFLAGS) $$truth_table $(COPTZ) $< life.c -o $@
endif # radix sort

life.gcda: prof.exe req-linux
ifeq ($(QUIET),true)
	./$< -Hn 10000000 frun &> /dev/null
	./$< -dn . 18446744073709551495 step 0xb9078411668e300d &> /dev/null
	./$< -n 7 step 0xb112a93586a4b278 &> /dev/null
ifneq ($(BWSEARCH),false)
	./$< -Hn 2 bus 0x5e315607a2200650 &> /dev/null
	./$< -qn 1 bus 0xffffffffffffffff &> /dev/null
	./$< -n 64 brun &> /dev/null
endif # BWSEARCH
	./$< -v &> /dev/null
else # QUIET == false
	./$< -Hn 10000000 frun
	./$< -dn . 18446744073709551495 step 0xb9078411668e300d
	./$< -n 7 step 0xb112a93586a4b278 &> /dev/null
ifneq ($(BWSEARCH),false)
	./$< -Hn 2 bus 0x5e315607a2200650
	./$< -qn 1 bus 0xffffffffffffffff
	./$< -n 64 brun
endif # BWSEARCH
	./$< -v
endif # QUIET
	@# just spam a bunch of flags to get profiling data on them
	./$< -adfsSTuQHRh @ . f1 0 90 f2 6,7

	mv prof-life.gcda $@

life.o: $(CFILES) life.gcda ruleset.tmp req-gcc
ifeq ($(ISA),native)
	@# this is only in this branch. the non-profiling branch can have the regular name
	truth_table=$(TRUTH_TABLE_CMD); \
	flags=$$(gcc -march=native -Q --help=target 2>/dev/null | awk '/enabled/ {print $$1}'); \
	isa=$$(for f in AVX512 ADX BMI2 AVX2 AVX SSE4.2 SSE4.1 SSSE3 SSE3 SSE2; do echo "$$flags" | grep -iq $$f && { echo $$f; break; }; done); \
	cpu=$$(wmic cpu get name | sed -n 2p | awk '{$$1=$$1; print}'); \
	gcc $(CFLAGS_LIFE_O) $$truth_table -DISA="\"native (ISA='$$isa', CPU='$$cpu')\"" $< -o $@.tmp
else
	truth_table=$(TRUTH_TABLE_CMD); \
	gcc $(CFLAGS_LIFE_O) $$truth_table $< -o $@.tmp
endif # ISA

	objcopy $@.tmp --remove-section .pdata --remove-section .xdata $@
	strip -S $@
endif # profile

ifeq ($(SHELL32),false)

life.exe: life.o radix-sort.o req-binutils req-vcbtools
ifeq ($(ISA),native)
	@# figure out if the native ISA has ADX or not.
	@if gcc -Q --help=target -march=native | grep -F adx | grep -Fq enabled; then \
		echo "ld $(LDFLAGS) life.o radix-sort.o $(LDLIBS) -o $@"; \
		ld $(LDFLAGS) life.o radix-sort.o $(LDLIBS) -o $@;        \
	else                                                          \
		echo "ld $(LDFLAGS) life.o $(LDLIBS) -o $@";              \
		ld $(LDFLAGS) life.o $(LDLIBS) -o $@;                     \
	fi;
else
	ld $(LDFLAGS) life.o $(LDLIBS) -o $@
endif # radix sort

else # SHELL32

life.exe: life.o init-crt.o radix-sort.o req-binutils req-vcbtools
ifeq ($(INCLUDE_RADIX),maybe)
	@if gcc -Q --help=target -march=native | grep -F adx | grep -Fq enabled; then \
		echo "ld $(LDFLAGS) life.o init-crt.o radix-sort.o $(LDLIBS) -o $@"; \
		ld $(LDFLAGS) life.o init-crt.o radix-sort.o $(LDLIBS) -o $@;        \
	else                                                                     \
		echo "ld $(LDFLAGS) life.o init-crt.o $(LDLIBS) -o $@";              \
		ld $(LDFLAGS) life.o init-crt.o $(LDLIBS) -o $@;                     \
	fi;
else
	ld $(LDFLAGS) life.o init-crt.o $(LDLIBS) -o $@
endif # radix sort

endif # SHELL32


ifneq ($(RESERVE),long)
	editbin -nologo -stack:32768,4096 $@
endif

life.txt: life.exe req-binutils req-vcbtools
	objdump -Mintel -D $< > $@
ifndef NO_VC
	echo "-----------------------------------" >> $@
	dumpbin -nologo -headers $< >> $@
endif

# wrapper program so double clicking from explorer does something other than exit immediately.

life-flaunch.o: life-flaunch.nasm req-nasm req-binutils
	nasm -fwin64 $< -o $@
	strip -S $@

life-flaunch.exe: life-flaunch.o req-binutils req-vcbtools
	ld $(LDFLAGS) $< -lucrtbase -lkernel32 -o $@
	strip -s $@
ifneq ($(RESERVE),long)
	editbin -nologo -heap:4096,4096 -stack:4096,4096 $@
endif

life-blaunch.o: life-blaunch.nasm req-nasm req-binutils
	nasm -fwin64 $< -o $@
	strip -S $@

life-blaunch.exe: life-blaunch.o req-binutils req-vcbtools
	ld $(LDFLAGS) $< -lucrtbase -lkernel32 -o $@
	strip -s $@
ifneq ($(RESERVE),long)
	editbin -nologo -heap:4096,4096 -stack:4096,4096 $@
endif

life-flaunch.txt: life-flaunch.exe req-binutils req-vcbtools
	objdump -Mintel -D $< > $@
ifndef NO_VC
	echo "-----------------------------------" >> $@
	dumpbin -nologo -headers $< >> $@
endif

life-blaunch.txt: life-blaunch.exe req-binutils req-vcbtools
	objdump -Mintel -D $< > $@
ifndef NO_VC
	echo "-----------------------------------" >> $@
	dumpbin -nologo -headers $< >> $@
endif

# cleanup stuff

clean: req-linux
	rm -f *.o *.tmp *.gcda prof.exe

distclean: req-linux
	rm -f *.o *.tmp *.gcda *.exe *.7z life.txt life-?launch.txt
