#pragma once
#define BW_STEP_H

// backwards stepping

#ifndef B_TT
	#pragma message("B_TT not defined. assuming B2")
	#define B_TT 0b000000100u
#endif

#ifndef S_TT
	#pragma message("S_TT not defined. assuming S23")
	#define S_TT 0b000001100u
#endif

// truth table to predecessor count (alive cells)
// \sum_{i=0}^8 (tt bit i) * (8 choose i)
#define TT2PDCRCNT_A(tt) ((u16) ( \
	((tt >> 0) & 1) *  1 + \
	((tt >> 1) & 1) *  8 + \
	((tt >> 2) & 1) * 28 + \
	((tt >> 3) & 1) * 56 + \
	((tt >> 4) & 1) * 70 + \
	((tt >> 5) & 1) * 56 + \
	((tt >> 6) & 1) * 28 + \
	((tt >> 7) & 1) *  8 + \
	((tt >> 8) & 1) *  1   \
))

// truth table to predecessor count (dead cells)
// 256 = 2^8 = \sum_{i=0}^8 (8 choose i)
#define TT2PDCRCNT_D(tt) ((u16) (256 - TT2PDCRCNT_A(tt)))

#define ALIVE_PRED_COUNT() (TT2PDCRCNT_A(B_TT) + TT2PDCRCNT_A(S_TT))
#define DEAD_PRED_COUNT()  (TT2PDCRCNT_D(B_TT) + TT2PDCRCNT_D(S_TT))

#define BW_SEARCH_ONES_BETTER()    (ALIVE_PRED_COUNT() < DEAD_PRED_COUNT())
#define BW_SEARCH_ZEROS_BETTER()   (ALIVE_PRED_COUNT() > DEAD_PRED_COUNT())

// NOTE: `size` never includes the metadata.

static void _heapify(u64 *arr, i64 n, i64 i) {
	i64 largest = i;

	while (true) {
		i64 left  = 2*i + 1;
		i64 right = 2*i + 2;

		if (left < n && arr[left] > arr[largest])
			largest = left;

		if (right < n && arr[right] > arr[largest])
			largest = right;

		if (largest == i)
			break;

		u64 tmp      = arr[i];
		arr[i]       = arr[largest];
		arr[largest] = tmp;
		i = largest;
	}
}

static void _sort(u64 *arr, i64 high, u64 depth, u64 depth_max) {
	// introsort

	const u64 entry_depth = depth;
	const u64 size = high + 1;

	while (high > 24) {
		if (depth > depth_max) {
			// heap sort
			for (i64 i = high + 1 >> 1; i --> 0 ;)
				_heapify(arr, high + 1, i);

			for (i64 i = high; i > 0; i--) {
				u64 tmp = arr[0];
				arr[0]  = arr[i];
				arr[i]  = tmp;

				_heapify(arr, i, 0ll);
			}

			break;
		}

		// quick sort
		i64 pi;

		const u64 p = ({
			// swap the median of three with the first element
			const u64
				a = arr[0],
				b = arr[high >> 1],
				c = arr[high];

			if ((b > a) ^ (b > c)) {
				// b is the median.
				arr[0]         = b;
				arr[high >> 1] = a;
			}
			else if ((c > a) ^ (c > b)) {
				// c is the median
				arr[0]    = c;
				arr[high] = a;
			}
			// else `arr[0]` is already the median. do nothing

			arr[0];
		});

		// compute the partition index
		u64 tmp;
		i64 i = 0, j = high;

		while (i < j) {
			while (arr[i] <= p && i <= high - 1) i++;
			while (arr[j] >  p && j >= 1) j--;

			if (i < j) {
				tmp    = arr[i];
				arr[i] = arr[j];
				arr[j] = tmp;
			}
		}

		tmp    = arr[0];
		arr[0] = arr[j];
		arr[j] = tmp;

		pi = j;

		depth++;
		// recurse on the shorter one
		if (pi < (high >> 1)) {
			_sort(arr, pi - 1, depth, depth_max);

			// _sort(arr + (pi + 1), high - (pi + 1), depth, depth_max);
			arr  += pi + 1;
			high -= pi + 1;
		}
		else {
			_sort(arr + (pi + 1), high - (pi + 1), depth, depth_max);

			// _sort(arr, pi - 1, depth, depth_max);
			high = pi - 1;
		}
	}

	if (entry_depth > 0)
		return;

	// delayed insertion sort pass
	for (u64 step = 1; step < size; step++) {
		u64 key = arr[step];
		i64 j   = (i64) (step - 1);

		while (j >= 0 && key < arr[j]) {
			arr[j + 1] = arr[j];
			j--;
		}

		arr[j + 1] = key;
	}
}

static void sort(u64 *const arr, const u64 size) {
	_sort(arr, (u64) (size - 1), 0, (63 - __builtin_clzll(size)) << 1);
}

static u64 uniq(Matx8 *const arr, const u64 size) {
	// deduplication in O(n log n) time
	// sort the data in ascending order. avoid function calls from libc `qsort`
	// puts("before quicksort");
	if (size < 2)
		return size;

	sort((u64 *) arr, size);

	// deduplicate any adjacent identical values
	u64 r = 1, w = 1; // read and write indices

	for (; r < size; r++)
		if (arr[r].matx != arr[w - 1].matx)
			// different from the last written value
			arr[w++] = arr[r];

	return w;
}

static Matx8 *_gen_combos(Matx8 *restrict out, Matx8 *restrict arr, Matx8 center, u8 n, u8 k) {
	// writes all k-combinations of partial states given the array of bits.

	// assumes out has space for least (n choose k) elements
	// returns a pointer to the next write location.
	// n is the length of the input array
	// each element in `arr`, should be something like `1llu << bit`
	// NOTE: `out` must point to somewhere in the program scratch buffer

	if (n > 8 || k > 8 || k > n)
		return out;

	if (k == 0) {
		*out = center;
		return out + 1;
	}

	u8 indices[8]; // k is never more than 8, so just allocate 8 bytes.

	// start with [0, 1, ..., k-1]
	for (u8 i = 0; i < k; i++)
		indices[i] = i;

	while (true) {
		// combine the states together and output the state
		if ((u64) (out - (Matx8 *) hashtable.scratch) >= SCRATCH_SIZE / sizeof(u64)) {
			// this shouldn't happen unless the hash table is tuned to be too small.
			// there shouldn't be more than a few hundred items.
			eputs("scratch buffer overflow: too many combos.");
			exit(ERRLOG_OOM_EC);
		}

		// add the partial state to the output list
		{
			Matx8 e = center;
			for (u8 i = 0; i < k; i++)
				e.matx |= arr[indices[i]].matx; // add the neighbors to the value

			*out++ = e;
		}

		// find the rightmost index to increment
		u8 i = k;
		while (i --> 0 && indices[i] == n - k + i);

		if (i == UINT8_MAX)
			break; // all combinations done

		// update all the indices
		for (u8 prev = ++indices[i++]; i < k; i++)
			indices[i] = ++prev;
	}

	return out;
}

static u64 states_for_bit(const u8 bit, const bool alive) {
	// writes the temporary states to the scratch buffer
	// does not add the bitmask to the start of the array
	// returns the number of objects it created
	// `alive` is the value of the current cell in the next state

	// x0 x1 x2
	// x3 x4 x5
	// x6 x7 x8
	const Matx8 x4 = {.matx = 1llu << bit};

	Matx8 *out = (Matx8 *) hashtable.scratch;

	constexpr u8 max_neighbors = 4 << (NEIGHBORHOOD == NH_MOORE);

	Matx8 nbuf[max_neighbors]; // neighbor buffer

	const Matx8 // center left, center right
		x3 = {.matx = (x4.matx >> 7) & 0x0101010101010101llu | (x4.matx << 1) & 0xfefefefefefefefellu},
		x5 = {.matx = (x4.matx >> 1) & 0x7f7f7f7f7f7f7f7fllu | (x4.matx << 7) & 0x8080808080808080llu};

#if NEIGHBORHOOD == NH_MOORE || NEIGHBORHOOD == NH_VON_NEUMANN
	const Matx8
		x1 = {.matx = ROL(x4.matx, 8)},
		x7 = {.matx = ROR(x4.matx, 8)};
#endif

#if NEIGHBORHOOD == NH_MOORE || NEIGHBORHOOD == NH_DIAGONAL
	const Matx8
		x0 = {.matx = ROL(x3.matx, 8)},
		x2 = {.matx = ROL(x5.matx, 8)},
		x6 = {.matx = ROR(x3.matx, 8)},
		x8 = {.matx = ROR(x5.matx, 8)};
#endif

	// TODO: it might be faster to store these in reverse order.
	//       probably runtime profiling would determine which is better.

#if NEIGHBORHOOD == NH_MOORE
	;nbuf[0] = x0; nbuf[1] = x1; nbuf[2] = x2;
	;nbuf[3] = x3;               nbuf[4] = x5;
	;nbuf[5] = x6; nbuf[6] = x7; nbuf[7] = x8;

	const Matx8 bits = {
		.matx = x0.matx | x1.matx | x2.matx | x3.matx | x4.matx |
				x5.matx | x6.matx | x7.matx | x8.matx
	};
#elif NEIGHBORHOOD == NH_VON_NEUMANN
	;              nbuf[0] = x1;            ;
	;nbuf[1] = x3;              nbuf[2] = x5;
	;              nbuf[3] = x7;            ;

	const Matx8 bits = {.matx =
		x1.matx | x3.matx | x4.matx | x5.matx | x7.matx
	};
#else // NH_DIAGONAL
	;nbuf[0] = x0;              nbuf[1] = x2;
	;                                       ;
	;nbuf[2] = x6;              nbuf[3] = x8;

	const Matx8 bits = {
		.matx = x0.matx | x2.matx | x4.matx | x6.matx | x8.matx
	};
#endif

	*out++ = bits;

	for (u8 neighbors = 0; neighbors <= max_neighbors; neighbors++) {
		// the XORs are there because the birth and survive numbers are for if
		// the next state's cell is alive. if it isn't, then the bits need to be flipped.

		// cases where it used to be dead (e.g. center == 0)
		if (((B_TT >> neighbors) & 1) ^ !alive)
			out = _gen_combos(out, nbuf, (Matx8) {0}, max_neighbors, neighbors);

		// cases where it used to be alive (e.g. center != 0)
		if (((S_TT >> neighbors) & 1) ^ !alive)
			out = _gen_combos(out, nbuf, x4, max_neighbors, neighbors);
	}

	// minus 1 because the 0th element is not part of the list
	return (u64) (out - (Matx8 *) hashtable.scratch) - 1;
}

static u64 advance_ptlstate(Matx8 **const pout, const u64 old_cnt, const u64 new_cnt, const u8 bit_idx) {
	// out = [allocation size] [bitmask] [actual output array]
	// NOTE: you have to call `states_for_bit` first, otherwise this returns garbage
	// NOTE: if old_cnt == 0, this will not do anything.
	// returns the new value of old_cnt
	// tmp1 is the scratch buffer with the new

	if (!cfg.quiet) {
		printf("advance_ptlstate(old_cnt=");
		print_du64(old_cnt, '_');
		printf(", new_cnt=");
		print_du64(new_cnt, '_');
		printf(", bit_idx=%u)\n", bit_idx);
	}

	if (new_cnt == 0)
		return 0;

	Matx8 *out = *pout;
	u64 out_size = out[-2].matx; // size in bytes

	// the values here should be set previously in states_for_bit
	Matx8 *new = (Matx8 *) hashtable.scratch + 1;

	const u64
		old_bitmask = out[-1].matx, // running partial state bitmask
		new_bitmask = new[-1].matx,  // bitmask for only the new bit
		shared_bitmask = old_bitmask & new_bitmask;

	out[-1].matx = old_bitmask | new_bitmask; // move the bitmask to the output and increment

	// point the temporary buffer right after the old buffer
	Matx8 *tmp = out + old_cnt;

	u64 idx = 0; // output index

	for (u64 i = 0; i < old_cnt; i++)
	for (u64 j = 0; j < new_cnt; j++)
		// for any bits that both states claim to know, they give the same values
		if ((shared_bitmask & (out[i].matx ^ new[j].matx)) == 0) {
			if ((idx + old_cnt + 3llu) * sizeof(Matx8) >= out_size) {
				out_size <<= 1; // double the size on each overflow
				out[-2].matx = out_size;

			#if DEBUG
				if (!cfg.quiet) enprintf(
					"doubling buffer size to %llu MiB. old_cnt=%llu, idx=%llu",
					out_size >> 20, old_cnt, idx
				);
			#endif

				out = realloc(out - 2, out_size);
				tmp = out + old_cnt;
				OOM(out, 1);

			#if DEBUG
				// sometimes the reallocation can take a long time.
				if (!cfg.quiet)
					puts(". done");
			#endif

				out += 2;
				*pout = out;
			}

			tmp[idx++].matx = out[i].matx | new[j].matx;
		}

	const u64 size = idx;

	memmove(out, tmp, size * sizeof(Matx8));
	return size;
}

constexpr u8 spread4_lut[16] = {
	0b00000000, 0b00000001, 0b00000100, 0b00000101,
	0b00010000, 0b00010001, 0b00010100, 0b00010101,
	0b01000000, 0b01000001, 0b01000100, 0b01000101,
	0b01010000, 0b01010001, 0b01010100, 0b01010101,
};

static FORCE_INLINE u16 spread8(const u8 x) {
	// double the bit indices for each bit in an 8-bit integer
	return spread4_lut[x >> 4] << 4 | spread4_lut[x & 15];
}

static FORCE_INLINE Matx8 *find_predecessors(Matx8 state) {
	// returns the number of predecessors found.
	// the elements are in the program scratch buffer
	static Matx8 *tmp = NULL;

	if (tmp == NULL) {
		// start off with 4 MiB
		tmp = malloc(4 * 1024 * 1024);
		OOM(tmp, 2);

		tmp += 2;
		tmp[-2].matx = 4 * 1024 * 1024;
	}

	// temporary buffer for the current bit's states
	tmp[-1].matx = 0; // no bits known
	tmp[0].matx  = 0; // blank state
	u64 cnt      = 1; // 1 object in the array

	struct { u8 x, y; } roll = {0};

	if (BW_SEARCH_ONES_BETTER() || BW_SEARCH_ZEROS_BETTER()) {
		Matx8 tmp;

		// find the best Y rotation
		// default to the worst value before finding better ones.
		u8 best_cnt = BW_SEARCH_ONES_BETTER() ? 0 : UINT8_MAX;

		for (u8 row = 0; row < 8; row++) {
			tmp = Matx8_yroll(state, row);
			const u8 cnt = POPCNT(tmp.quarters[0]);

			if (BW_SEARCH_ONES_BETTER() ? cnt > best_cnt : cnt < best_cnt) {
				roll.y   = row;
				best_cnt = cnt;
			}
		}

		state = tmp;

		// find the best X rotation
		// default to the worst value before finding better ones
		u16 best_val = BW_SEARCH_ONES_BETTER() ? UINT16_MAX : 0;

		for (u8 col = 0; col < 8; col++) {
			tmp = Matx8_xroll(state, col);
			// row 1: A B C D E F G H
			// row 2: I J K L M N O P
			// =>
			//    A   B   C   D   E   F   G   H
			// or   I   J   K   L   M   N   O   P
			//  =
			//    A I B J C K D L E M F N G O H P
			const u16 val = spread8(tmp.rows[1]) << 1 | spread8(tmp.rows[0]);

			if (BW_SEARCH_ONES_BETTER() ? val < best_val : val > best_val) {
				roll.x   = col;
				best_val = val;
			}
		}

		state = tmp;
	}

	if (!cfg.quiet)
		printf("preroll = (%u, %u)\n", roll.x, roll.y);

	// TODO: for the diagonal only, do a completely different state index map than this.
	//       you want to make "2x2" diamonds instead of boxes there.

	u8 bit_idx = 0;

	// form as many 2x2 boxes as possible as early as possible (reduce the search space)
	for (u8 i = 0; i < 8; i++) {
		cnt = advance_ptlstate(
			&tmp, cnt,
			states_for_bit(i, (state.matx >> i) & 1),
			bit_idx++
		);

		if (cnt == 0)
			goto done;

		cnt = advance_ptlstate(
			&tmp, cnt,
			states_for_bit(i + 8, (state.matx >> i + 8) & 1),
			bit_idx++
		);

		if (cnt == 0)
			goto done;
	}

	// starting here, almost all cells form a 2x2 box just by iterating forwards.
	for (u8 i = 16; i < 64 && cnt > 0; i++)
		cnt = advance_ptlstate(
			&tmp, cnt,
			states_for_bit(i, (state.matx >> i) & 1),
			bit_idx++
		);

	// transfer the data from `tmp` to `out`
	cnt = uniq(tmp, cnt);
done:
	// print the final object count
	advance_ptlstate(NULL /*unneeded argument*/, cnt, 0, 64);

	// this should be a compile-time condition
	if (BW_SEARCH_ONES_BETTER() || BW_SEARCH_ZEROS_BETTER()) {
		roll.x = (8 - roll.x) & 7;
		roll.y = (8 - roll.y) & 7;

		if (roll.x > 7 || roll.y > 7)
			__builtin_unreachable();

		// I don't think the order matters, but undo
		// them in reverse order just in case.

		if (roll.x != 0)
			for (u64 i = 0; i < cnt; i++)
				tmp[i] = Matx8_xroll(tmp[i], roll.x);

		if (roll.y != 0)
			for (u64 i = 0; i < cnt; i++)
				tmp[i] = Matx8_yroll(tmp[i], roll.y);
	}

	Matx8 *out = (Matx8 *) malloc((cnt + 1) * sizeof(Matx8)) + 1;
	OOM(out, 3);

	out[-1].matx = cnt;
	memcpy(out, tmp, cnt * sizeof(Matx8));

	return out;
}

static FORCE_INLINE Matx8 *advance_predecessors(const Matx8 *const prev_states) {
	Matx8 *running_states = (Matx8 *) malloc(2 * sizeof(Matx8)) + 1;
	u64 running_cnt = 0;

	for (u64 i = 0; i < prev_states[-1].matx; i++) {
		const Matx8 *new_states = find_predecessors(prev_states[i]);

		// append the new states to the running states list
		const u64 new_cnt = new_states[-1].matx;
		if (new_cnt > 0) {
			const u64 total_size = 1 + running_cnt + new_cnt;
			running_states = (Matx8 *) realloc(running_states - 1, total_size * sizeof(Matx8)) + 1;
			OOM(running_states, 4);
			memcpy(running_states + running_cnt, new_states, new_cnt * sizeof(Matx8));
			running_cnt = total_size - 1;
		}

		free((void *) (new_states - 1));
	}

	running_states[-1].matx = uniq(running_states, running_cnt);
	free((void *) (prev_states - 1));
	return running_states;
}
