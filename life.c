// compile with `make -B CLIP=true SHELL32=false NEIGHBORHOOD=MOORE RULESET=B3/S23 ISA=adx BENCH=false`

// I do not care that writing to u.x and then reading from u.y is not defined
// for unions. C just reinterprets the bits as the other type with no change,
// even if the standard doesn't technically guarantee that is what happens.

// TODO: put the ruleset somewhere in the data.json file.
//       in the python file, only collapse objects with the same ruleset
//       or maybe include the ruleset in the filename?
// TODO: given a list of states A1, A2, ..., AN, determine if there is a ruleset
//       under which state A1 => A2, A2 => A3, etc.
// TODO: figure out why -R is slower than -H, and then either remove it or make it not slower.
// TODO: implement reading from the pipeline. also figure out what it would be reading.
//       perhaps a state list?

///////////////////////////////// config start ////////////////////////////////

#ifndef ALIVE_CHAR_DEF
	#define ALIVE_CHAR_DEF	'#' // character to print for alive cells
#endif
#ifndef DEAD_CHAR_DEF
	#define DEAD_CHAR_DEF	' ' // character to print for dead cells
#endif

#ifndef SLEEP_MS_T_DEF
	#define SLEEP_MS_T_DEF	1500
#endif
#ifndef SLEEP_MS_S_DEF
	#define SLEEP_MS_S_DEF	90
#endif

// period after which `nrun inf` logs the summary and resets.
// granularity lower than like 250 likely won't do anything.
// the timer is only checked every 267,378,720 trials. (INT16_MAX * UINT8_MAX * 4 * 8)
// 43200 == 12*60*60. this definition has to be the final expression result.
#ifndef TIMER_PERIOD
	#define TIMER_PERIOD	43200 // seconds
#endif

// step value threshold after which `life step` will start using modulo on the count
// 512-1024 is around the ballpark of when it starts giving improvement
#ifndef STEP_MOD_THRESH
	#define STEP_MOD_THRESH	512
#endif

// NOTE: these values and comments are for NEIGHBORHOOD=MOORE and RULESET=B3/S23
//       other ones may need way more memory for stuff like the transients
//       for example, VON_NEUMANN with B23/S23 needs TRANSIENT_LEN >= 2048

// use 8 for hyperthreading. 9  the fastest on a single core.
// unless your L1 cache is 64KiB, in which case 9 or maybe even 10 is probably better.
// this has to be at least 2, or the program will not work.
#ifndef TABLE_BITS
	#define TABLE_BITS		9
#endif

// 512 makes them one page of memory.
// these have to be at least 133 and 424 respectively.
#ifndef PERIOD_LEN
	#define PERIOD_LEN		136
#endif
#ifndef TRANSIENT_LEN
	#define TRANSIENT_LEN	448
#endif

#ifndef RAND_BUF_LEN
	#define RAND_BUF_LEN	128
#endif

// max number of collisions per trial before errors.
// this has to be at least 143 with TABLE_BITS=9
#ifndef ARENA_LEN
	#define ARENA_LEN		256
#endif

#ifndef PY_BASE
	// base name of the python file
	#define PY_BASE "analyze"
#endif

#ifndef DATAFILE
	#define DATAFILE "data.json"
#endif

#ifndef OUTFILE
	// only for 'step' and 'bwsr' commands.
	#define OUTFILE "out.txt"
#endif

#ifndef CLIPBOARD
	// true  => include the -c flag
	// false => no -c flag (less DLL imports)
	#define CLIPBOARD false
#endif

#ifndef HELP
	// true => include help text in the binary.
	// false => don't include help text. significantly reduces the binary size.
	#define HELP true
#endif

#ifndef BWSEARCH
	// true => include bwsr and bwrn commands
	// false => don't.
	#define BWSEARCH true
#endif

#ifndef WRAPPER
	// true => dump, merg, fold, and cnt commands
	// false => don't.
	#define WRAPPER true
#endif

#ifndef DEBUG
	// true => print extra collision data when the program exits.
	// false => don't.
	#define DEBUG false
#endif

#ifndef SHELL32
	// default to using init_args with SHELL32.dll
	#define SHELL32 true
#endif

#ifndef PROFILING
	#define PROFILING false
#endif

////////////////////////////////// config end /////////////////////////////////

#ifdef _MSC_VER
	// I don't care if you are using clang-cl or something. that counts in my eyes.
	#error Silly Microsoft sheep. Visual Studio will not work here. Use a real C compiler.
#endif

#ifndef _WIN64
	// NOTE: windows is always little endian, so I don't have to check that as well.
	#error This program will only compile on 64-bit windows
#endif

#ifndef __MINGW64__
	#error This program will only compile properly with a MinGW compiler.
#endif

#ifndef __GNUC__
	#error "This program only works with compilers that allow GNU extensions."
#endif

#ifndef VERSION
	#error "VERSION must be defined"
#endif

#define     ARENA_MAX (-1 +     ARENA_LEN)
#define    PERIOD_MAX (-1 +    PERIOD_LEN)
#define TRANSIENT_MAX (-1 + TRANSIENT_LEN)

#if PROFILING
	#define access _access
#else
	// NOTE: the profiling mode is strange, because it automatically
	//       links with msvcrt, and if you pass `-nostdlib -ffreestanding`,
	//       then it just says it can't find any of the functions it needs,
	//       even if you do `-Wl,-lucrtbase` or something.

	// undefine the references to the __mingw_* nonsense
	// and add prototypes for functions specific to UCRT
	#define _UCRT

	#define access _access_s

	// this has to be before the includes. The headers don't actually
	// define _crt_at_quick_exit before the includes so the compiler prototypes it for me.
	#define atexit		_crt_at_quick_exit
	#define exit		quick_exit
	#define strtoull	_strtoui64 // these are the same underlying function anyway

	// this shouldn't do anything since _UCRT is defined,
	// but I really don't want it to use the __mingw functions.
	#define __USE_MINGW_ANSI_STDIO 0
#endif

#define FALLTHROUGH __attribute__((fallthrough))

#define keypressed(key) (GetAsyncKeyState(key) & 0x8000)
#define ememcpy(dst, src, len) (__builtin_memcpy(dst, src, len) + len /* point to the end */)
#define streq(x, y) (__builtin_strcmp(x, y) == 0)
#define POPCNT(x) __builtin_stdc_count_ones(x)
#define ROL(x, n) __builtin_stdc_rotate_left(x, n)
#define ROR(x, n) __builtin_stdc_rotate_right(x, n)
#define POP_ARG() (argc--, *argv++ /* return what was just popped */)
#define TOSTRING(x) #x
#define TOSTRING_EXPANDED(x) TOSTRING(x)
#define INT_LEN(x) (      \
	(x) <      10 ? 1 :   \
	(x) <     100 ? 2 :   \
	(x) <   1'000 ? 3 :   \
	(x) <  10'000 ? 4 :   \
	(x) < 100'000 ? 5 : 6 \
)

// use _previous_ to access the original value from within the block.
// the expression returns the temporary value of `thing`.
// you can use either `with_return;` or `goto _restore_;` to restore the
// original value and then return
#define with_return() goto _restore_
#define with(thing, tmp_val, block) ({    \
	const __auto_type _previous_ = thing; \
	thing = tmp_val;                      \
	block;                                \
_restore_: (void) &&_restore_;            \
	thing = _previous_;                   \
	tmp_val;                              \
})

// static branch prediction hinting is still used to build prof.exe.
// NOTE: these default branch probability is 90%

#define likely(x)     __builtin_expect(!!(x), 1)
#define likelyp(x, p) __builtin_expect_with_probability(!!(x), 1, p)

#define unlikely(x)     __builtin_expect(!!(x), 1)
#define unlikelyp(x, p) __builtin_expect_with_probability(!!(x), 0, p)

#define likely_if(x)       if (likely(x))
#define unlikely_if(x)     if (unlikely(x))
#define likelyp_if(x, p)   if (likelyp(x, p))
#define unlikelyp_if(x, p) if (unlikelyp(x, p))

// p is the chance that it stays in the loop
#define likely_while(x)       while (likely(x))
#define unlikely_while(x)     while (unlikely(x))
#define likelyp_while(x, p)   while (likelyp(x, p))
#define unlikelyp_while(x, p) while (unlikelyp(x, p))

#define until(x) while (!(x))

// p is the chance that it exits
#define likely_until(x)       while (likely(!(x)))
#define unlikely_until(x)     while (unlikely(!(x)))
#define likelyp_until(x, p)   while (likelyp(!(x), p))
#define unlikelyp_until(x, p) while (unlikelyp(!(x), p))

// NOTE: little endian integers are stored backwards.
#define _1CHARS_TO_U08(c0) ((u8) c0)
#define _2CHARS_TO_U16(c0, c1) ((u16)c1 << 8 | (u16)c0)
#define _4CHARS_TO_U32(c0, c1, c2, c3) ((u32)c3 << 24 | (u32)c2 << 16 | (u32)c1 << 8 | (u32)c0)
#define _8CHARS_TO_U64(c0, c1, c2, c3, c4, c5, c6, c7) \
	((u64)_4CHARS_TO_U32(c4, c5, c6, c7) << 32 | (u64)_4CHARS_TO_U32(c0, c1, c2, c3))

#include <string.h>      // strcmp, sprintf, memcpy
#include <time.h>        // _localtime64, _timespec64_get, struct _timespec64, struct tm
#include <sys/stat.h>    // S_IWRITE
#include <sys/locking.h> // LK_NBLCK
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h> // io.h (_open, _write, ...), O_CREAT, ...

// EXIT_SUCCESS == 0
// EXIT_FAILURE == 1
#define EXIT_DATAFILE		2 // something about accessing the datafile
#define EXIT_CMD_INVOP		3 // invalid command operand (or wrong number)
#define EXIT_FLG_INVOP		4 // invalid flag operand (or wrong number)
#define EXIT_CMD_UNKWN		5 // unknown or blank command
#define EXIT_FLG_UNKWN		6 // unknown or blank flag
#define EXIT_OOM 			7 // out of heap memory

#undef stdout
#undef stderr
static FILE *stdout = NULL, *stderr = NULL;

#define ERRLOG_USE_RUNTIME_LOG_LEVEL
#define ERRLOG_OOM_EC EXIT_OOM
#include "errlog.h"
#include "windows.h"
#include "matx8.h"
#include "matx8-next.h" // Matx8_next
#include "table.h"

// 2d 8-bit point
typedef struct {
	u8 x, y;
} Point8;

typedef enum <% EMPTY, CONST, CYCLE %> sttyp_t;          // state type
typedef enum { N_UNUSED, N_USED, N_INFINITE } nstatus_t; // `n` status

// hashtable and total_collisions are defined in matx8-table.h now.

// static HashTable hashtable = {0};
#define COMBINED_HIST_SIZE (PERIOD_LEN + TRANSIENT_LEN + 2)
#define DATA_SIZE ((COMBINED_HIST_SIZE + 1) * sizeof(u64))
static union {
	char raw[DATA_SIZE];

	struct {
		u64 combined[COMBINED_HIST_SIZE];
		u64 trial;
	};

	__attribute__((packed)) struct {
		u64 periods[PERIOD_LEN];
		u64 transients[TRANSIENT_LEN];
		u64 counts[3]; // EMPTY, CONST, CYCLE
	};
} __attribute__((aligned(64))) data = {0};

#if DEBUG
static u64 max_collisions_state = 0; // the state with the most hash collisions
static u32 max_collisions       = 0; // max collisions in a single trial
// static u64 total_collisions  = 0; // total collisions across all trials
#endif

static struct {
	// these pairs are unions so I don't have to make the
	// struct packed and do extra stuff to make it work

	u64 n;

	union {
		struct { u32 trial, state; };   // 8 bytes
		u32 array[2];
	} sleep_ms;

	union {
		struct { u8 stop, update; };    // 2 bytes
		u8 array[2];
	} keys;

	union {
		struct { char dead, alive; };   // 2 bytes
		char array[2];
		u16 raw;
	} sim_chars;

	nstatus_t nstatus;

	bool file_out, bell, silent, quiet; // 4 bytes

#if CLIPBOARD
	bool clip;                          // 1 byte
#endif
} cfg = {
	.n         = 0,
	.sleep_ms  = {.trial = SLEEP_MS_T_DEF, .state  = SLEEP_MS_S_DEF},
	.keys      = {.stop  = VK_F1,          .update = VK_INSERT     },
	.sim_chars = {.alive = ALIVE_CHAR_DEF, .dead   = DEAD_CHAR_DEF },
	.nstatus    = N_UNUSED,
};


#if HELP
static const char *const help_string =
	"life v" VERSION
	"\nusage: life [FLAGS] COMMAND [OPERANDS]"
	"\n"
	"\nflags:"
	"\n    -n   specify an integer count argument for commands that take it."
	"\n         'inf' can be given for infinity."
	"\n    -a   specify a character to print for dead cells in sim modes."
	"\n    -d   specify a character to print for alive cells in sim modes."
	"\n    -b   print a bell character when the program exits."
#if CLIPBOARD
	"\n    -c   in run modes, copy the summary to the clipboard as well as printing."
#endif
	"\n    -f   in run modes, concatenate the summary data into " DATAFILE "."
	"\n         in 'step' and 'bwsr' commands, data is placed into " OUTFILE "."
	"\n    -R   use REALTIME process priority class and lock to the given CPU cores."
	"\n         outside of administrator mode, REALTIME does the same thing as HIGH priority."
	"\n         the argument can be a hex or binary core mask, or a core list like \"1,2,3\"."
	"\n         if a mask of 0 is given, the CPU affinity is not set."
	"\n    -H   use HIGH process priority class."
	"\n    -q   quiet mode. suppresses most non-error output messages."
	"\n    -Q   silent mode. suppresses all terminal output including error messages."
	"\n    -s   specify a key code to stop in applicable modes"
	"\n    -u   specify a key code to update the user in `-n inf frun`."
	"\n    -S   specify a wait in ms between states in sim modes. default=" TOSTRING_EXPANDED(SLEEP_MS_S_DEF) "."
	"\n    -T   specify a wait in ms between trials in sim modes. default=" TOSTRING_EXPANDED(SLEEP_MS_T_DEF) "."
	"\n    -v   print the version string and exit."
	"\n    -h, -?, --help   print this message and exit"
	"\n"
	"\n    key codes for -s and -u can be an integer or a string. integer codes are here:"
	"\n     - https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes"
	"\n    string codes for the top key row are as follows: f0 for Esc, f1-f9, fA-fC"
	"\n    for F10-F12, and fD-fJ for the remaining keys. F13-F24 are given as 'f+', 'f,',"
	"\n    'f-', 'f.', 'f/', 'f:', 'f;', 'f<', 'f=', 'f>', 'f?', and 'f@', respectively. If"
	"\n    fa-fz are given, then the second character (as uppercase) is used as the keycode,"
	"\n    e.g `-s fc` is the same as `-s 67`. Also, fK: control, fL: lmouse, fM: mid-mouse,"
	"\n    fN: r-mouse, fO: side mouse 1, fP: side mouse 2, fQ: backspace, fR is return"
	"\n    (enter), fS: space, fT: tab."
	"\n"
	"\n    flags can be coalesced together, e.g. `-fsquT <s> <u> <T>`."
	"\n    flags must appear before the command."
	"\n"
	"\ncommands:"
	"\n    help         alias of `-h` flag"
	"\n    frun [S...]  runs simulations on given states and returns data histograms"
	"\n                 also runs N Monte-Carlo trials if `-n` is used."
	"\n    sim [S...]   runs simulations visually on all given states"
	"\n                 also runs N Monte-Carlo trials if `-n` is used."
	"\n    sim1 [S]     traverse the given state until the stop key is pressed"
	"\n    step [S...]  step to find the Nth-generation descendant to all given states."
	"\n                 N is given through `-n`."
	"\n    show [S...]  the same as `step`, but forces N=0."
	"\n    rand         print out random states."
#if BWSEARCH
	"\n    bwsr [S...]  backwards search to find all Nth-generation ancestors to all given"
	"\n                 states. N is given through `-n`. 'bus' is an alias command"
	"\n    brun         runs N Monte-Carlo trials on predecessor searches and counts results."
	"\n                 N is given through `-n`."
#endif
	"\n    tfm S [T] [X Y]  apply an optional transformation (T) and an optional translation"
	"\n                     (X, Y). options for T are given below. T happens before X and Y."
#if WRAPPER
	"\n"
	"\n    dump         runs `./" PY_BASE ".py -s " DATAFILE "` and exits"
	"\n    fold         runs `./" PY_BASE ".py -f " DATAFILE "` and exits"
	"\n    cnt          counts the number of objects in " DATAFILE
	"\n    merg A B     runs `./" PY_BASE ".py -m A B` and exits"
#endif
	"\n"
	"\n    N always defaults to 1 if not given."
	"\n    S defaults to a random state if optional and not given."
	"\n    frun and sim stop traversing when a repeat state is found."
#if BWSEARCH
	", sim, and brun"
#else
	" and sim"
#endif
	" to make them run until the stop key is given."
	"\n"
	"\nbuild config:"
	#ifdef ISA // the profiling version doesn't always have this
	"\n    ISA=\""			ISA "\""
	#endif
	"\n    PGO=\""			PGO "\""
	"\n    NEIGHBORHOOD="
	#if NEIGHBORHOOD == NH_MOORE
		"MOORE"
	#elif NEIGHBORHOOD == NH_VON_NEUMANN
		"VON_NEUMANN"
	#elif NEIGHBORHOOD == NH_DIAGONAL
		"DIAGONAL"
	#endif
	"\n    RULESET=\""		RULESET "\""
	"\n    TIMER_PERIOD="	TOSTRING_EXPANDED(TIMER_PERIOD)
	"\n    TABLE_BITS="		TOSTRING_EXPANDED(TABLE_BITS)
	"\n    PERIOD_LEN="		TOSTRING_EXPANDED(PERIOD_LEN)
	"\n    TRANSIENT_LEN="	TOSTRING_EXPANDED(TRANSIENT_LEN)
	#if RAND_BUF_LEN == 1
	"\n    RAND=\"RDRAND, unbuffered\""
	#else
	"\n    RAND=\"RtlGenRandom, buffer=" TOSTRING_EXPANDED(RAND_BUF_LEN) "\""
	#endif
	"\n    ARENA_LEN="		TOSTRING_EXPANDED(ARENA_LEN)
	"\n    CLIPBOARD="		TOSTRING_EXPANDED(CLIPBOARD)
	"\n    BWSEARCH="		TOSTRING_EXPANDED(BWSEARCH)
	"\n    WRAPPER="		TOSTRING_EXPANDED(WRAPPER)
	"\n    DEBUG="			TOSTRING_EXPANDED(DEBUG)
	"\n    SHELL32="		TOSTRING_EXPANDED(SHELL32)
	"\n"
	"\nexit codes:"
	"\n    " TOSTRING_EXPANDED(EXIT_FAILURE)   "  generic error, likely propogated from a subprocess."
	"\n    " TOSTRING_EXPANDED(EXIT_DATAFILE)  "  could not perform an operation on the datafile for an unknown reason"
	"\n    " TOSTRING_EXPANDED(EXIT_CMD_INVOP) "  command given with invalid operands or the wrong amount of operands"
	"\n    " TOSTRING_EXPANDED(EXIT_FLG_INVOP) "  flag given with invalid operands or the wrong amount of operands"
	"\n    " TOSTRING_EXPANDED(EXIT_CMD_UNKWN) "  an unknown or empty command was given"
	"\n    " TOSTRING_EXPANDED(EXIT_FLG_UNKWN) "  an unknown or empty flag was given"
	"\n    " TOSTRING_EXPANDED(EXIT_OOM)       "  program is out of heap memory"
	"\n"
	"\nstate interest bit meanings (for run modes):"
	"\n    7  end state is not empty and is a perfect inverse of the start state"
	"\n    6  end state is not empty. start and end states together total the board"
	"\n    5  constant end state with a number of alive bits in (26, 32)"
	"\n    4  new period value"
	"\n    3  new transient value"
	"\n    2  period > 36 and transient - period > 196"
	"\n    1  2nd or 3rd encounter of a particular period"
	"\n    0  2nd or 3rd encounter of a particular transient length"
	"\n"
	"\ntransformation codes:"
	// NOTE: these might be out of order, but always internally consistent
	"\n    " TOSTRING_EXPANDED(TFM_IDENTITY)	"  " TFM_IDENTITY_STR
	"\n    " TOSTRING_EXPANDED(TFM_YFLIP)		"  " TFM_YFLIP_STR
	"\n    " TOSTRING_EXPANDED(TFM_XFLIP)		"  " TFM_XFLIP_STR
	"\n    " TOSTRING_EXPANDED(TFM_TRS)			"  " TFM_TRS_STR
	"\n    " TOSTRING_EXPANDED(TFM_ANTI_TRS)	"  " TFM_ANTI_TRS_STR
	"\n    " TOSTRING_EXPANDED(TFM_ROT180)		"  " TFM_ROT180_STR
	"\n    " TOSTRING_EXPANDED(TFM_ROT90)		"  " TFM_ROT90_STR
	"\n    " TOSTRING_EXPANDED(TFM_ROT270)		"  " TFM_ROT270_STR
;
#else // HELP
static const char *const help_string = "help text was not included in this build";
#endif

__attribute__((optimize("unroll-loops")))
static FORCE_INLINE void print_table_headers(void) {
	if (cfg.quiet)
		return;

#if INT_LEN(PERIOD_LEN) == 3 && INT_LEN(TRANSIENT_LEN) == 3
	printf(
		"timestamp        | start state        | int | per | trs | n | trial\n"
		"--------------------------------------------------------------------"
	);
#else
	printf(
		"timestamp        | start state        | int | %*s | %*s | n | trial\n"
		"--------------------------------------------------------------------",
		INT_LEN(PERIOD_LEN), "per", INT_LEN(TRANSIENT_LEN), "trs"
	);

	for (u8 i = 0; i < max(INT_LEN(PERIOD_LEN), 3) + max(INT_LEN(TRANSIENT_LEN), 3) - 6; i++)
		putchar('-');
#endif
}

static void show_cursor(void) { likely_if (!cfg.silent) printf("\e[?25h"); }
static void bell(void) { likely_if (!cfg.silent) putchar('\x07'); }

#include "du64.h"
#include "summary.h"
#include "sim.h"
#include "run.h"

#if BWSEARCH
	#include "bw-search.h"
	#include "bw-run.h"
#endif

#define SUMMARY_IMPL
#include "summary.h"

#if DEBUG
static void log_collisions(void) {
	if (cfg.quiet || total_collisions == 0)
		return;

	printf("hash collisions: total="); print_du64(total_collisions, '_');
	printf(", trial max=%u, s=0x%016zx\n", max_collisions, max_collisions_state);
}
#endif

static FORCE_INLINE bool parse_flags(u32 *const restrict pargc, char **restrict *const restrict pargv) {
	// assumes neither argument is null. if you pass null, then you are stupid.

	// modifies the arguments and also modifies global state.
	// NOTE: this uses goto, but the label is always to an exit routine
	//       and it never jumps backwards in the code.

	u32    argc = *pargc;
	char **argv = *pargv;

	char fc, *flag, *full_flag, *operand;

	if (argc == 0 || **argv != '-')
		return false;

	do { // for each flag set
		full_flag = POP_ARG();
		flag = full_flag + 1; // first argument, but skip the dash.

		// --help is the only flag that can be more than one character
		unlikely_if (streq(flag, "-help"))
			goto help_flag;

		fc = *flag; // flag character

		unlikely_if (fc == '\0')
			goto flag_empty;

		for (; fc != '\0'; fc = *++flag) { // for each flag in the flag set
			// NOTE: if you do something like `-T -q`, then the `-q` will be the
			//       operand to `-T`, and that will be an error because it isn't
			//       a valid value, and not because it looks like a flag.
			operand = argc > 0 ? *argv : NULL;

			switch (fc) {
			case 'n':
				unlikely_if (operand == NULL)
					goto flag_no_operand;

				if (streq(operand, "inf"))
					// cfg.n can be left as whatever
					cfg.nstatus = N_INFINITE;
				else {
					char *arg_end;
					const u64 n = strtoull(operand, &arg_end, 0);

					unlikely_if (*arg_end != '\0')
						goto flag_invalid_operand;

					cfg.n       = n;
					cfg.nstatus = N_USED;
				}

				POP_ARG();
				break;
			case 'Q':
				cfg.silent   = true;
				ERRLOG_LEVEL = ERRLOG_NONE;
				FALLTHROUGH; // also set quiet to true
			case 'q': cfg.quiet    = true; break;
			case 'b': cfg.bell     = true; break;
			case 'f': cfg.file_out = true; break;
			#if CLIPBOARD
			case 'c': cfg.clip     = true; break;
			#endif
			case 'T': FALLTHROUGH;
			case 'S': {
				unlikely_if (operand == NULL)
					goto flag_no_operand;

				_Static_assert(&cfg.sleep_ms.trial == cfg.sleep_ms.array,
					"the trial sleep time must come first in the structure.");
				u32 *const pvar = cfg.sleep_ms.array + (fc == 'S');

				char *arg_end;
				const u64 tmp = strtoull(operand, &arg_end, 0);

				unlikely_if (*arg_end != '\0')
					goto flag_invalid_operand;

				*pvar = tmp > UINT32_MAX ? UINT32_MAX : tmp;
				POP_ARG();
				break;
			}
			case 's': FALLTHROUGH;
			case 'u': {
				unlikely_if (operand == NULL)
					goto flag_no_operand;

				_Static_assert(&cfg.keys.stop == cfg.keys.array,
					"the stop key must come first in the structure.");
				u8 *const pkey = cfg.keys.array + (fc == 'u');
				// u8 *const pkey = fc == 'u' ? &cfg.keys.update : &cfg.keys.stop;

				// allow strings top row keys and some software-only keys
				likely_if (operand[0] == 'f' && operand[1] && !operand[2]) {
					// function key
					switch (operand[1]) {
					// software-only function keys f13-f17
					case '+': *pkey = VK_F13; break;
					case ',': *pkey = VK_F14; break;
					case '-': *pkey = VK_F15; break;
					case '.': *pkey = VK_F16; break;
					case '/': *pkey = VK_F17; break;
					// top row keys
					case '0': *pkey = VK_ESCAPE; break;
					case '1': *pkey = VK_F1; break;
					case '2': *pkey = VK_F2; break;
					case '3': *pkey = VK_F3; break;
					case '4': *pkey = VK_F4; break;
					case '5': *pkey = VK_F5; break;
					case '6': *pkey = VK_F6; break;
					case '7': *pkey = VK_F7; break;
					case '8': *pkey = VK_F8; break;
					case '9': *pkey = VK_F9; break;
					// software-only function keys f18-f24
					case ':': *pkey = VK_F18; break;
					case ';': *pkey = VK_F19; break;
					case '<': *pkey = VK_F20; break;
					case '=': *pkey = VK_F21; break;
					case '>': *pkey = VK_F22; break;
					case '?': *pkey = VK_F23; break;
					case '@': *pkey = VK_F24; break;
					// the rest of the function row keys
					case 'A': *pkey = VK_F10; break;
					case 'B': *pkey = VK_F11; break;
					case 'C': *pkey = VK_F12; break;
					case 'D': *pkey = VK_INSERT; break;
					case 'E': *pkey = VK_SNAPSHOT; break; // print screen
					case 'F': *pkey = VK_DELETE; break;
					case 'G': *pkey = VK_HOME; break;
					case 'H': *pkey = VK_END; break;
					case 'I': *pkey = VK_PRIOR; break; // page up
					case 'J': *pkey = VK_NEXT; break;  // page down
					// stuff other than top row keys
					case 'K': *pkey = VK_CONTROL; break;
					case 'L': *pkey = VK_LBUTTON; break; // left mouse button
					case 'M': *pkey = VK_MBUTTON; break; // middle mouse button
					case 'N': *pkey = VK_RBUTTON; break; // right mouse button
					case 'O': *pkey = VK_XBUTTON1; break; // side mouse button 1
					case 'P': *pkey = VK_XBUTTON2; break; // side mouse button 2
					case 'Q': *pkey = VK_BACK; break; // backspace
					case 'R': *pkey = VK_RETURN; break; // enter
					case 'S': *pkey = VK_SPACE; break;
					case 'T': *pkey = VK_TAB; break;
					default :
						// parse stuff like -s fn as -s 110
						// basically make it uppercase and use that as the key code
						likely_if ('a' <= operand[1] && operand[1] <= 'z') {
							*pkey = operand[1] & ~32; // set uppercase
							break;
						}
						else
							goto flag_invalid_operand; // print a message and exit.
					}
				}
				else {
					// argument is not a function key
					char *arg_end;
					const u64 vkey = strtoull(operand, &arg_end, 0);

					unlikely_if (*arg_end != '\0' || vkey > UINT8_MAX)
						goto flag_invalid_operand;

					*pkey = vkey;
				}

				POP_ARG();
				break;
			}
			case 'a': FALLTHROUGH;
			case 'd': {
				char *const pchar = cfg.sim_chars.array + (fc == 'a');

				unlikely_if (operand == NULL)
					goto flag_no_operand;

				unlikely_if (*operand == '\0')
					goto flag_invalid_operand;

				// NOTE: characters after the first are ignored.
				*pchar = *operand;
				POP_ARG();
				break;
			}
			case 'R': {
				unlikely_if (operand == NULL)
					goto flag_no_operand;

				char *arg_end;

				u64 cpu_affinity_mask;

				// NOTE: leading whitespace will cause masks to be read as a core index.

				// only parse the mask directly if the input is hex or binary
				if (operand[0] == '0' && (operand[1] | 32) == 'x')
					cpu_affinity_mask = strtoull(operand, &arg_end, 0);
				else if (operand[0] == '0' && (operand[1] | 32) == 'b')
					cpu_affinity_mask = strtoull(operand + 2, &arg_end, 2);
				else {
					// parse a logical core list, e.g. `-R "2,5,61"`
					cpu_affinity_mask = 0;

					// use a separate pointer in case the affinity mask is invalid later.
					// flag_invalid_operand will need the pointer to the start of the string.
					char *operand_cur = operand;
					do {
						const u64 core = strtoull(operand_cur, &arg_end, 0);

						unlikely_if (core > 63)
							goto flag_invalid_operand;

						// NOTE: passing something like `-R "2,2,2,2,2,2"` is the same as `-R 2`
						cpu_affinity_mask |= 1llu << core;
						operand_cur = arg_end + 1;
					} while (*arg_end == ',');
				}

				unlikely_if (*arg_end != '\0')
					goto flag_invalid_operand;

				void *const process = GetCurrentProcess();
				SetPriorityClass(process, REALTIME_PRIORITY_CLASS);

				// affinity
				if (likely(cpu_affinity_mask != 0) && unlikely(!SetProcessAffinityMask(process, cpu_affinity_mask)))
					goto flag_invalid_operand;

				POP_ARG();
				break;
			}
			case 'H':
				SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
				break;
			case 'h': FALLTHROUGH;
			case '?':
				goto help_flag;
			case 'v':
			#if HELP
				// NOTE: this works because `help_string` starts with `"life v" VERSION`
				likely_if (!cfg.quiet)
					printf("%.*s\n", (i32) (__builtin_strlen("life v") + __builtin_strlen(VERSION)), help_string);
				else likely_if (!cfg.silent)
					printf("%.*s\n", (i32) __builtin_strlen(VERSION), help_string + __builtin_strlen("life v"));
			#else
				// store a separate string from the help text.
				likely_if (!cfg.quiet)
					puts("life v" VERSION);
				else likely_if (!cfg.silent)
					puts("life v" VERSION + __builtin_strlen("life v"));
			#endif
				exit(EXIT_SUCCESS);
			default:
				goto flag_unknown;
			} // switch, (decide what flag operation to dispatch)
		} // for, (iterate flag characters)
	} while (argc > 0 && **argv == '-'); // while, (iterate arguments)

	*pargc = argc;
	*pargv = argv;
	return true;

help_flag:
	likely_if (!cfg.silent)
		puts(help_string);
	exit(EXIT_SUCCESS);

flag_no_operand:
	eprintf("flag `-%c` (in `%s`) given without an operand.\n", *flag, full_flag);
	exit(EXIT_FLG_INVOP);

flag_invalid_operand:
	eprintf("flag `-%c` (in `%s`) given with an invalid value `%s`\n", *flag, full_flag, operand);
	exit(EXIT_FLG_INVOP);

flag_empty:
#if HELP
	eputs("empty flag given. use command `-h` for help.");
#else
	eputs("empty flag given.");
#endif

	exit(EXIT_FLG_UNKWN);
flag_unknown:
#if HELP
	eprintf("unknown flag `-%c` (in `%s`). use command `-h` for help.\n", *flag, full_flag);
#else
	eprintf("unknown flag `-%c` (in `%s`).\n", *flag, full_flag);
#endif

	exit(EXIT_FLG_UNKWN);
}

static _Noreturn void cmd_invalid_operand(const char *const restrict cmd, const u8 pos) {
	eprintf("command `%s` given with an invalid value at position %u.\n", cmd, pos);
	exit(EXIT_CMD_INVOP);
}

static Matx8 Matx8_tryparse(
	char *restrict *restrict argv,
	const char *const restrict cmd,
	const u8 pos
) {
	// this can't really go in `matx8.h` because it is specific to parsing from argv
	char *str_end, *str = argv[pos];

	// NOTE: if you pass something like "  0b10101", then it will not work as expected
	//       because I do not skip leading whitespace before the "0b" check.
	const u64 state = str[0] == '0' && (str[1] | 32) == 'b' ?
		strtoull(str + 2, &str_end, 2) :
		strtoull(str, &str_end, 0);

	if (*str_end != '\0')
		cmd_invalid_operand(cmd, pos);

	return (Matx8) {.matx = state};
}

#if SHELL32
void init_crt(void);
#else
#include <corecrt_startup.h>
#endif

#if PROFILING
int main(void)
#else
// profiling for the main function is discarded because I couldn't get it to work.
void mainCRTStartup(void)
#endif
{
	// I don't care that `int` is actually i32.
	// nobody is ever passing enough arguments that it matters.
	u32 argc;
	char **argv;

#if SHELL32
	// I can't get rid of this entirely because the profiling build can't
	// call the UCRTBASE initialization functions
	asm volatile (
		"call init_args\n\t"
		"mov %0, edi\n\t"
		"mov %1, rsi"
		: "=r"(argc), "=r"(argv)
		: // no inputs
		: "rax", "rcx", "rdx", "r8", "r9", "rdi", "rsi", "cc", "memory"
	);
#else
	_initialize_narrow_environment();
	_configure_narrow_argv(_crt_argv_unexpanded_arguments);

	argc = *__p___argc();
	argv = *__p___argv();
#endif

	stdout = __acrt_iob_func(1);
	stderr = __acrt_iob_func(2);

	POP_ARG(); // the executable path is not needed.
	do { // do while false
		const bool flags_given = parse_flags(&argc, &argv);

		likely_if (argc != 0)
			// there are more arguments left
			break;

		// example: `./life` should print the help text, but `./life -H` shouldn't
		if (flags_given) {
		#if HELP
			eputs("no command given. use `-h` for help.");
		#else
			eputs("no command given.");
		#endif
			exit(EXIT_CMD_UNKWN);
		}
		else {
			// no arguments given. just print the help text.
			likely_if (!cfg.silent)
				puts(help_string);
			exit(EXIT_SUCCESS);
		}
	} while (false);

	if (_isatty(1) && likely(!cfg.silent)) {
		printf("\e[0m\e[?25l"); // remove terminal styling if there is any and hide the cursor.
		atexit(&show_cursor);
	}

#if DEBUG
	atexit(&log_collisions);
#endif

	if (cfg.bell)
		atexit(&bell);

	// NOTE: all the commands are at least 3 characters long, so if any of the
	//       first 3 characters are null, then it is definitely not a known commands
	unlikely_if (argv[0][0] == '\0' || argv[0][1] == '\0' || argv[0][2] == '\0')
		goto unknown_command;

	// the string is longer than 4 characters, so it is definitely unknown
	unlikely_if (argv[0][3] && argv[0][4])
		goto unknown_command;

	if (argc == 0)
		__builtin_unreachable();

	// parse the first argument as a 32-bit unsigned integer.
	// these branches aren't worth putting in helper functions because they are tiny.
	// NOTE: this is safe because the string is at least 3 characters long,
	//       so it takes up at least 4 bytes including the null terminator.
	switch (*(u32 *) *argv) {
	case _4CHARS_TO_U32('f', 'r', 'u', 'n'):
		print_table_headers();

		// parse states
		for (u32 i = 1; i < argc; i++) {
			unlikely_if (cfg.silent)
				run_once(Matx8_tryparse(argv, "frun", i));
			else {
				putchar('\n');
				Matx8 state = Matx8_tryparse(argv, "frun", i);
				printf("\e[A");
				run_once(state);
			}
		}

		// execute the numeric part
		likely_if (cfg.nstatus == N_INFINITE) {
			run_forever();
			give_summary(SUM_NO_RETURN);
			__builtin_unreachable();
		}

		if (cfg.nstatus == N_UNUSED && argc == 1)
			// if `-n X` isn't passed and also no states were passed, default to 1 trial
			cfg.n = 1;

		for (u8 i = 0; i < (cfg.n & 7); i++)
			run_once();

		for (u64 i = cfg.n >> 3; i --> 0 ;)
			RUN_8();

		give_summary(SUM_NO_RETURN);
		__builtin_unreachable();
	case _4CHARS_TO_U32('s', 'i', 'm', 0 ):
		// run once for each state given
		for (u32 i = 1; i < argc; i++) {
			cli_sim(i, Matx8_tryparse(argv, "sim", i));

			unlikely_if (i != argc - 1)
				// don't sleep after the last iteration
				Sleep(cfg.sleep_ms.trial);
		}

		if (cfg.nstatus == N_INFINITE) {
			u64 trial = 1;

			while (true) {
				cli_sim(trial++);
				Sleep(cfg.sleep_ms.trial);
			}

			__builtin_unreachable();
		}

		if (cfg.nstatus == N_UNUSED && argc == 1)
			// if `-n X` isn't passed and also no states were passed, default to 1 trial
			cfg.n = 1;


		for (u64 i = 1; i < cfg.n; i++) {
			cli_sim(i);
			Sleep(cfg.sleep_ms.trial);
		}

		// do the last simulation without a sleep after it.
		if (cfg.n != 0)
			cli_sim(cfg.n);

		#if DEBUG
		// print the collisions data on its own line
		if (!cfg.quiet)
			putchar('\n');
		#endif

		break;
	case _4CHARS_TO_U32('s', 'i', 'm', '1'):
		if (argc == 1) {
			cli_sim_one();
			exit(EXIT_SUCCESS);
		}

		unlikely_if (argc > 2) {
			eprintf("command `%s` expected %s operands, found %u.\n", "sim1", "0 or 1", argc - 1);
			exit(EXIT_CMD_INVOP);
		}

		// argc == 2
		cli_sim_one(Matx8_tryparse(argv, "sim1", 1));
		break;
	case _4CHARS_TO_U32('s', 'h', 'o', 'w'):
		cfg.nstatus = N_USED;
		cfg.n       = 0;

		// this is basically a branch fallthrough
		goto step_start_doing_stuff;
	case _4CHARS_TO_U32('s', 't', 'e', 'p'): {
		if (cfg.nstatus == N_UNUSED)
			// default to 1 step of not given.
			cfg.n = 1;
		else if (cfg.nstatus == N_INFINITE)
			// this has to be set here so it goes into the modulo branch.
			cfg.n = ~0llu;

	step_start_doing_stuff:
		if (--argc == 0)
			break;

		// argc is now the amount of states given

		u32 fd;

		if (cfg.file_out) {
			fd = _open(OUTFILE, O_CREAT | O_WRONLY | O_BINARY, S_IWRITE);

			unlikelyp_if (fd == ~0u, 0.9999999) {
				i32 error; _get_errno(&error);
				eprintf("can't %s %s: errno=%u.\n", "open", OUTFILE, error);
				exit(EXIT_DATAFILE);
			}

			unlikely_if (_locking(fd, LK_NBLCK, INT32_MAX) != 0) {
				// only try once because I don't feel like looping.
				i32 error; _get_errno(&error);
				eprintf("can't %s %s: errno=%u.\n", "lock", OUTFILE, error);
				exit(EXIT_DATAFILE);
			}

			_lseeki64(fd, 0, SEEK_END);
		}

		for (u32 si = 1; si <= argc; si++) {
			Matx8 state = Matx8_tryparse(argv, "step", si);
			Matx8 orig_state = state;
			u64 n = cfg.n; // local value of n.
			u32 p;

			unlikely_if (cfg.n >= STEP_MOD_THRESH) {
				{
					const bool original_quiet = cfg.quiet;
					cfg.quiet = true;
					run_once(state);
					cfg.quiet = original_quiet;
				}

				u32 t;

				for (p = 0; data.periods[p] == 0; p++)
					if (p >= PERIOD_MAX)
						__builtin_unreachable();

				for (t = 0; data.transients[t] == 0; t++)
					if (t >= TRANSIENT_MAX)
						__builtin_unreachable();

				// these could also be decrements, which would be ever so slightly slower,
				// but would have smaller opcodes. I think this is better for this case.
				data.periods[p]    = 0;
				data.transients[t] = 0;

				unlikelyp_if (cfg.nstatus == N_INFINITE && p != 1, 0.9999999)
					goto step_handle_indeterminate;

				n -= t; // subtract the steps already made
				t -= p; // don't iterate the period part. (not required)
				n %= p; // modulo the remaining count by the period

				// iterate until it gets to the start of the loop
				while (t --> 0)
					state = Matx8_next(state);
			}

			while (n --> 0)
				state = Matx8_next(state);

			// the compiler doesn't realize that fd is only used after it is initialized
			// because cfg.file_out doesn't change between the two points.

			// like it is `if (condition) fd = whatever;`
			// then `if (condition) do_stuff(fd);` with the same condition.
			// fuckass retarded compiler
			#pragma GCC diagnostic push
			#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
			if (cfg.file_out) _write(
				fd,
				hashtable.scratch,
				sprintf(hashtable.scratch, "0x%016zx => 0x%016zx\n" + 12, state.matx)
			);
			#pragma GCC diagnostic pop

			if (!cfg.quiet)
				printf("0x%016zx => 0x%016zx\n", orig_state.matx, state.matx);
			else likely_if (!cfg.silent)
				printf("0x%016zx => 0x%016zx\n" + 12, state.matx);

			if (!cfg.quiet) {
				print_state(state);
				putchar('\n');

				if (si < argc)
					putchar('\n');
			}

			continue;

		step_handle_indeterminate:
			#pragma GCC diagnostic push
			#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
			if (cfg.file_out) _write(fd, "0x%016zx => @__indeterminate__\n" + 12, 19);
			#pragma GCC diagnostic pop

			if (!cfg.quiet)
				printf("0x%016zx => @__indeterminate__\n", orig_state.matx);
			else likely_if (!cfg.silent)
				printf("0x%016zx => @__indeterminate__\n" + 12);

			if (!cfg.quiet) {
				// figure out which cells are constant and which ones are
				// actually indeterminate.

				Matx8 diff_mask = {0}, next_state;
				for (u32 i = 0; i < p; i++) {
					next_state = Matx8_next(state);
					diff_mask.matx |= state.matx ^ next_state.matx;

					state = next_state;
				}

				print_state(state, diff_mask);
				putchar('\n');

				if (si < argc)
					putchar('\n');
			}
		} // for

	#if DEBUG
		// stop it from printing the collision count at the end.
		// that information is only useful for `run` and `nrun` modes.
		total_collisions = 0;
	#endif

		// the OS will unlock and close the file itself
		break;
	}
	case _4CHARS_TO_U32('r', 'a', 'n', 'd'):
		if (cfg.nstatus == N_UNUSED && argc == 1)
			cfg.n = 1;

		if (cfg.nstatus == N_INFINITE) {
			while (true) {
				const Matx8 state = Matx8_random();
				if (!cfg.silent)
					printf("0x%016zx => 0x%016zx\n" + 12, state.matx);

				if (!cfg.quiet) {
					print_state(state);
					putchar('\n');
					putchar('\n');
				}
			}

			__builtin_unreachable();
		}

		while (cfg.n --> 0) {
			const Matx8 state = Matx8_random();
			if (!cfg.silent)
				printf("0x%016zx => 0x%016zx\n" + 12, state.matx);

			if (!cfg.quiet) {
				print_state(state);
				putchar('\n');

				if (cfg.n != 0)
					putchar('\n');
			}
		}
		break;
#if BWSEARCH
	case _4CHARS_TO_U32('b', 'u', 's',  0 ): FALLTHROUGH;
	case _4CHARS_TO_U32('b', 'w', 's', 'r'): {
		// backwards search

		if (cfg.nstatus == N_INFINITE)
			// I don't want to implement the infinite one, and this will take like a hundred billion years
			// to finish, so this might as well be infinite. Also, some states will go into cycles, and I
			// don't really have a good way of storing that information without taking up an exponential
			// amount of memory with respect to n. I would have to store the list of states with each
			// depth, and iterate through it at each depth, checking for matching values against every
			// single previous depth, and that would be insanely slow and cause OOM crashes quite quickly.
			cfg.n = ~0llu;
		else if (cfg.nstatus == N_UNUSED)
			cfg.n = 1;

		StateBuffer *pdlist;

		if (--argc == 0) // set to the number of arguments to the command.
			// if no values are given, do nothing.
			break;

		_Static_assert(sizeof(StateBuffer) == 1*sizeof(u64),
			"state buffer should only have a `size` attribute");

		pdlist = (StateBuffer *) malloc(sizeof(StateBuffer) + argc*sizeof(Matx8));
		pdlist->size = argc;

		for (u32 i = 0; i < argc; i++)
			pdlist->states[i] = Matx8_tryparse(argv, "bwsr", i + 1);

		for (; cfg.n > 0 && pdlist->size > 0; cfg.n--) {
			if (!cfg.quiet)
				printf("%zu step%s remaining\n", cfg.n, "s" + (cfg.n == 1));

			pdlist = (StateBuffer *) find_predecessors((const StateBuffer *) pdlist);
		}

		if (cfg.file_out) {
			const u32 fd = _open(OUTFILE, O_CREAT | O_WRONLY | O_BINARY, S_IWRITE);

			unlikelyp_if (fd == ~0u, 0.9999999) {
				i32 error; _get_errno(&error);
				eprintf("can't %s %s: errno=%u.\n", "open", OUTFILE, error);
				exit(EXIT_DATAFILE);
			}

			unlikely_if (_locking(fd, LK_NBLCK, INT32_MAX) != 0) {
				// only try once because I don't feel like looping.
				i32 error; _get_errno(&error);
				eprintf("can't %s %s: errno=%u.\n", "lock", OUTFILE, error);
				exit(EXIT_DATAFILE);
			}

			_lseeki64(fd, 0, SEEK_END);

			*(u16 *) hashtable.scratch = _2CHARS_TO_U16('0', 'x');

			for (u64 i = 0; i < pdlist->size; i++) _write(
				fd,
				hashtable.scratch,
				2 + sprintf(hashtable.scratch + 2, "0x%016zx => 0x%016zx\n" + 12 + 2, pdlist->states[i].matx)
			);
		}

		likely_if (!cfg.silent)
			for (u64 i = 0; i < pdlist->size; i++)
				printf("0x%016zx => 0x%016zx\n" + 12, pdlist->states[i].matx);

		break;
	}
	case _4CHARS_TO_U32('b', 'r', 'u', 'n'):
		// backwards run
		init_bws_hist2();

		// TODO: also accept states for brun, as well as the numbers. currently, I don't think the
		//       bws-run module allows for this, just based on how the functions are implemented.

		unlikely_if (argc > 1) {
			eprintf("command `%s` expected %s operands, found %u.\n", "brun", "0", argc - 1);
			exit(EXIT_CMD_INVOP);
		}

		likely_if (!cfg.quiet)
			// don't worry about overflowing the table.
			// very few states have more than 10^17 predecessors.
			printf(
				"timestamp        | start state        | predecessor count | trial\n"
				"------------------------------------------------------------------"
			);

		if (cfg.nstatus == N_INFINITE)
			bws_run_forever();
		else likely_if (cfg.nstatus == N_USED)
			bws_run(cfg.n);
		else
			bws_run(1);

		give_summary(SUM_NO_RETURN, SUM_BACKWARDS);
		__builtin_unreachable();
#endif
	case _4CHARS_TO_U32('t', 'f', 'm',  0 ): {
		unlikely_if (argc < 2 && argc > 5) {
			eprintf("command `%s` expected %s operands, found %u.\n", "tfm", "1-4", argc - 1);
			exit(EXIT_CMD_INVOP);
		}

		Matx8 state = Matx8_tryparse(argv, "tfm", 1);
		u64 tfm = 0;
		Point8 roll = {0};

		/*if (argc == 2) {
			// [0=cmd 1=state]
			// nothing
		}
		else */
		if (argc == 3) {
			// [0=cmd 1=state 2=tfm]
			tfm = Matx8_tryparse(argv, "tfm", 2).matx;
		}
		else if (argc == 4) {
			// [0=cmd 1=state 2=xroll 3=yroll]
			roll.x = Matx8_tryparse(argv, "tfm", 2).rows[0];
			roll.y = Matx8_tryparse(argv, "tfm", 3).rows[0];
		}
		else if (argc == 5) {
			// [0=cmd 1=state 2=tfm   3=xroll 4=yroll]
			tfm   = Matx8_tryparse(argv, "tfm", 2).matx;
			roll.x = Matx8_tryparse(argv, "tfm", 3).rows[0];
			roll.y = Matx8_tryparse(argv, "tfm", 4).rows[0];
		}

		unlikely_if (tfm >= sizeof(tfm_strs) / sizeof(*tfm_strs))
			cmd_invalid_operand("tfm", 2);

		unlikely_if (roll.x > 7) cmd_invalid_operand("tfm", 2 + (argc == 5));
		unlikely_if (roll.y > 7) cmd_invalid_operand("tfm", 3 + (argc == 5));

		state = Matx8_tfm(state, tfm);
		state = Matx8_xroll(state, roll.x);
		state = Matx8_yroll(state, roll.y);

		likely_if (!cfg.silent)
			printf("0x%016zx => 0x%016zx\n" + 12, state.matx);

		if (!cfg.quiet) {
			print_state(state);
			putchar('\n');
		}

		break;
	}
#if WRAPPER
	case _4CHARS_TO_U32('d', 'u', 'm', 'p'):
		exit(system(PY_BASE " -s " DATAFILE));
		__builtin_unreachable();
	case _4CHARS_TO_U32('f', 'o', 'l', 'd'):
		exit(system(PY_BASE " -f " DATAFILE));
		__builtin_unreachable();
	case _4CHARS_TO_U32('m', 'e', 'r', 'g'): {
		unlikely_if (argc != 3) {
			eprintf("command `%s` expected %s operands, found %u.\n", "merg", "2", argc - 1);
			exit(EXIT_CMD_INVOP);
		}

		// I don't want a DLL import for strnlen just for this.
		// limit paths to 255 characters.
		for (u8 i, j = 1; j < 3; j++) {
			for (i = 0; i < UINT8_MAX && argv[j][i] != '\0'; i++);

			unlikely_if (i == UINT8_MAX)
				cmd_invalid_operand("merg", i);
		}

		// allocate on the stack instead of using `malloc`.
		// also round up to the next multiple of 16, because I decided.
		char command[(__builtin_strlen(PY_BASE " -m ") + UINT8_MAX + 1/*space*/ + UINT8_MAX + 1/*null*/ + 15) & ~15];

		sprintf(command, PY_BASE " -m %s %s", argv[1], argv[2]);

		exit(system(command));
		__builtin_unreachable();
	}
	case _4CHARS_TO_U32('c', 'n', 't',  0 ): {
		const u32 fd = _open(DATAFILE, O_RDONLY | O_BINARY);
		unlikely_if (fd == ~0u) {
			i32 error; _get_errno(&error);
			eprintf("can't %s %s: errno=%u.\n", "read", DATAFILE, error);
			exit(EXIT_DATAFILE);
		}

		unlikely_if (_locking(fd, LK_NBLCK, INT32_MAX) != 0) {
			// only try once because I don't feel like looping.
			i32 error; _get_errno(&error);
			eprintf("can't %s %s: errno=%u.\n", "lock", DATAFILE, error);
			exit(EXIT_DATAFILE);
		}

		// the number of characters in the sequence matched at the end of the previous buffer
		u8 tmp_len = 0;

		u32 objects = 0;
		i32 len;
		char *const restrict buf = hashtable.scratch;

		// NOTE: it doesn't make sense for a file to end with "\n{\n\t".
		//       without at least like 20 or so extra characters.
		_Static_assert(SCRATCH_SIZE > 15, "SCRATCH_SIZE should be at least like 16.");

		while ((len = _read(fd, buf, SCRATCH_SIZE)) > 8) {
			i32 i = 0;

			switch (tmp_len) {
			default: __builtin_unreachable();
			case 0: break;
			case 1:
				if (*(u16 *)buf == _2CHARS_TO_U16('{', '\n') && buf[2] == '\t') {
					objects++;
					i = 3;
				}
				else
					i = 1; // start at the second newline
				break;
			case 2:
				objects += *(u16 *)buf == _2CHARS_TO_U16('\n', '\t');
				i = 2;
				break;
			case 3:
				objects += *buf == '\t';
				i = 1;
				break;
			}

			for (; i <= len - 4; i++)
				objects += *(u32 *)(hashtable.scratch + i) ==
							_4CHARS_TO_U32('\n', '{', '\n', '\t');

			if (*(u16 *)(buf + len - 3) == _2CHARS_TO_U16('\n', '{') &&
				hashtable.scratch[len - 1] == '\n')
				tmp_len = 3;
			else if (*(u16 *)(buf + len - 2) == _2CHARS_TO_U16('\n', '{'))
				tmp_len = 2;
			else if (buf[len - 1] == '\n')
				tmp_len = 1;
			else
				tmp_len = 0;
		}

		unlikely_if (len == -1) {
			i32 error; _get_errno(&error);
			eprintf("can't %s %s: errno=%u.\n", "read", DATAFILE, error);
			exit(EXIT_DATAFILE);
		}

		likely_if (!cfg.silent)
			printf(cfg.quiet ? "%u\n" : "found %u objects\n", objects);
		break;
	}
#endif
	case _4CHARS_TO_U32('h', 'e', 'l', 'p'):
		likely_if (!cfg.silent)
			puts(help_string);
		break;
	default:
	unknown_command:
	#if HELP
		eprintf("unknown command `%s`. use `-h` for help.\n", *argv);
	#else
		eprintf("unknown command `%s`.\n", *argv);
	#endif
		exit(EXIT_CMD_UNKWN);
	}

	exit(EXIT_SUCCESS);
}
