#pragma once
#define BW_RUN_H

#define BWS_HIST_PRIMARY_KEY (~0llu)

// the main histogram is in `data.combined` for low value keys,
// and `bws_hist2` stores the sparse values for the remaining keys

typedef struct {
	u64 key, cnt;
} bws_hist_entry;

// backup histograms for backwards search
static struct {
	u64 max_size;
	u64 size;
	bws_hist_entry list[];
} *bws_hist2 = NULL;

static FORCE_INLINE void init_bws_hist2(void) {
	bws_hist2 = malloc(2*sizeof(u64) + 32*sizeof(bws_hist_entry));
	bws_hist2->max_size = 32;
	bws_hist2->size     =  0;
}

static u64 bws_increment_hist_value(const u64 key) {
	if (key < COMBINED_HIST_SIZE) {
		data.combined[key]++;
		return BWS_HIST_PRIMARY_KEY;
	}

	const u64 i = ({
		// find the index of the key, or if it isn't in the list,
		// find the index of where it would be if it was.
		u64 lo = 0, hi = bws_hist2->size;

		while (lo < hi) {
			const u64 mid = lo + (hi - lo >> 1);

			if (bws_hist2->list[mid].key < key)
				lo = mid + 1;
			else
				hi = mid;
		}

		lo;
	});

	if (bws_hist2->list[i].key == key) {
		// element already exists
		bws_hist2->list[i].cnt++;
		return i;
	}

	// resize if needed
	if (bws_hist2->size == bws_hist2->max_size) {
		// 50% increase each time it runs out of space
		bws_hist2->max_size = bws_hist2->max_size*3 >> 1;
		bws_hist2 = realloc(bws_hist2, 2*sizeof(u64) + bws_hist2->max_size*sizeof(bws_hist_entry));
	}

	memmove(
		bws_hist2->list + i + 1,
		bws_hist2->list + i,
		(bws_hist2->size - i) * sizeof(bws_hist_entry)
	);

	bws_hist2->list[i] = (bws_hist_entry) {
		.key = key,
		.cnt = 1
	};

	bws_hist2->size++;

	return i;
}

static void _bws_run_once2(const bool quiet, const Matx8 state) {
	// this assumes cfg.quiet was already force set to true.
	// the `quiet` argument is used for logging

	const u64 key = find_predecessors(state)->size;
	const u64  i  = bws_increment_hist_value(key);

	if (quiet)
		return;

	const u64 c = i == BWS_HIST_PRIMARY_KEY ?
		data.combined[key] :
		bws_hist2->list[i].cnt;

	data.trial++;

	// print interesting states
	// state, number of predecessors, instance count
	if (c > 5)
		return;

	// the zero initialization is required so the program doesn't crash in profiling mode.
	struct _timespec64 ts = {0};

#if PROFILING
	asm volatile (              // 13 byte NOP
		".fill 12, 1, 0x66\n\t" // o16 prefix, with 11 more redundant copies.
		".byte 0x90\n\t"        // one byte nop
	);
#else
	_timespec64_get(&ts, TIME_UTC);
#endif

	struct tm *tm = _localtime64(&ts.tv_sec);

	// timestamp is the same format as for the run commands

	printf("\n%03d-%02d:%02d:%02d.%03ld | 0x%016llx | %17llu | ",
		tm->tm_yday + 1,      // DAY: 001-366
		tm->tm_hour,          // HH: 00-23
		tm->tm_min,           // MM
		tm->tm_sec,           // SS
		ts.tv_nsec / 1000000, // MS
		state.matx, key
	);

	print_du64(data.trial);
}

static FORCE_INLINE void _bws_run_once1(const bool quiet) {
	_bws_run_once2(quiet, Matx8_random());
}

#define bws_run_once(quiet, state...) \
	VA_IF(_bws_run_once2(quiet, state), _bws_run_once1(quiet), state)

static void bws_run(u64 n) {
	const bool original_quiet = cfg.quiet;
	cfg.quiet = true;

	while (n --> 0)
		bws_run_once(original_quiet, Matx8_random());

	cfg.quiet = original_quiet;
}

static void bws_run_forever(void) {
	bool update_pressed = false;

	const bool original_quiet = cfg.quiet;
	cfg.quiet = true;

	while (true) {
		// these can take a long time, so check keys after every call
		bws_run_once(original_quiet);

		// if you are pressing both keys, print the trial first and then exit.
		unlikely_if (GetAsyncKeyState(cfg.keys.update) & 0x8000) {
			if (!update_pressed && likely(!cfg.silent)) {
				putchar(original_quiet ? '\r' : '\n');
				printf("trial: ");
				print_du64(data.trial);
			}

			update_pressed = true;
		} else
			update_pressed = false;

		unlikelyp_if (GetAsyncKeyState(cfg.keys.stop) & 0x8000, 0.9999) {
			cfg.quiet = original_quiet;
			return;
		}
	}
}
