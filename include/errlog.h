#pragma once
#define ERRLOG_H

// requires C23 for __VA_OPT__

#ifdef ERRLOG_NO_ANSI
	// use empty strings for the same semantics
	#define RGB_STR(r, g, b)  ""
	#define ANSI_COLOR(color) ""
	#define ANSI_RESET()      ""

	// `(void) 0` instead of nothing so you still need to use a semicolon.
	#define FCON_COLOR(color) ((void) 0)
	#define FCON_RESET()      ((void) 0)
#else
	#define RGB_STR(r, g, b)  #r ";" #g ";" #b
	#define ANSI_COLOR(color) "\e[38;2;" color "m"
	#define ANSI_RESET()      "\e[0m"

	#define FCON_COLOR(fp, color) fprintf(fp, ANSI_COLOR(color))
	#define FCON_RESET(fp)        fprintf(fp, ANSI_RESET())
#endif

#define ANSI_RED    RGB_STR(166,  12,  26) // #A60C1A
#define ANSI_ORANGE RGB_STR(180, 100,   0) // #B46400
#define ANSI_YELLOW RGB_STR(160, 160,   0) // #A0A000
#define ANSI_GREEN  RGB_STR( 10, 128,  10) // #0A800A
#define ANSI_BLUE   RGB_STR(25 , 127, 250) // #197FFA
#define ANSI_WHITE  RGB_STR(255, 255, 255) // #FFFFFF
#define ANSI_BLACK  RGB_STR(  0,   0,   0) // #000000

// this is basically the same as the values in Python's `logging` package.
#define ERRLOG_NONE  5u // don't print any error messages
#define ERRLOG_FATAL 4u // only print fatal error messages
#define ERRLOG_WARN  3u // print fatal error messages and warnings
#define ERRLOG_NOTE  2u // print fatal, warning, and note error messages
#define ERRLOG_DEBUG 1u // print fatal, warning, note, and debug error messages
#define ERRLOG_ALL   0u // print everything. functionally equivalent to ERRLOG_DEBUG

#ifdef ERRLOG_USE_RUNTIME_LOG_LEVEL
	// this has to be explicitly turned on, mostly because I don't want it on.
	static unsigned char /* uint8_t */ ERRLOG_LEVEL = ERRLOG_NOTE;
#else
	#define ERRLOG_LEVEL ERRLOG_NOTE
#endif

#ifndef ERRLOG_OOM_EC
	// the program return code to be given on out-of-memory errors.
	#define ERRLOG_OOM_EC 12 // same as ENOMEM from <errno.h>
#endif

// internal color print versions
#define _FPRINTF_COLOR(fp, color, loglvl, args...) ({ \
	ERRLOG_LEVEL > (loglvl) ? 0 : ({                  \
		FCON_COLOR(fp, color);                        \
		const int return_code = fprintf((fp), args);  \
		FCON_RESET(fp);                               \
		return_code;                                  \
	});                                               \
})

// external color print versions
#define efprintf(fp, args...)  _FPRINTF_COLOR(fp, ANSI_RED   , ERRLOG_FATAL, args)
#define ewfprintf(fp, args...) _FPRINTF_COLOR(fp, ANSI_ORANGE, ERRLOG_WARN , args)
#define enfprintf(fp, args...) _FPRINTF_COLOR(fp, ANSI_YELLOW, ERRLOG_NOTE , args)
#define edfprintf(fp, args...) _FPRINTF_COLOR(fp, ANSI_BLUE  , ERRLOG_DEBUG, args)

#define eprintf(args...)   efprintf(stderr, args)
#define ewprintf(args...) ewfprintf(stderr, args)
#define enprintf(args...) enfprintf(stderr, args)
#define edprintf(args...) edfprintf(stderr, args)

#define efputs(fp, str)  _FPRINTF_COLOR(fp, ANSI_RED   , ERRLOG_FATAL, "%s\n", str)
#define ewfputs(fp, str) _FPRINTF_COLOR(fp, ANSI_ORANGE, ERRLOG_WARN , "%s\n", str)
#define enfputs(fp, str) _FPRINTF_COLOR(fp, ANSI_YELLOW, ERRLOG_NOTE , "%s\n", str)
#define edfputs(fp, str) _FPRINTF_COLOR(fp, ANSI_BLUE  , ERRLOG_DEBUG, "%s\n", str)

#define eputs(str)   efputs(stderr, str)
#define ewputs(str) ewfputs(stderr, str)
#define enputs(str) enfputs(stderr, str)
#define edputs(str) edfputs(stderr, str)

#define OOM(mem, code) ({                    \
	if ((mem) == NULL) {                      \
		eprintf("Out of Memory. code: %llu\n", \
			(unsigned long long int) (code));   \
		exit(ERRLOG_OOM_EC);                     \
	}                                             \
})

#define _VA_ID_IGNORED(...)
#define _VA_ID(x...) x
#define _VA_ID_IF(suffix, x...) _VA_ID ## suffix(x)
#define VA_IF(t, f, ...) __VA_OPT__(t) _VA_ID_IF(__VA_OPT__(_IGNORED), f)
// use VA_IF for macro overloading, like for 1-arg and a 2-arg versions of a macro.
// e.g. #define f(x, y...) VA_IF(f2(x, y), f1(x), y)
