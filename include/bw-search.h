#pragma once
#define BW_SEARCH_H

// backwards search

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
#define TT2PDCNT_A(tt) ((u16) ( \
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
#define TT2PDCNT_D(tt) ((u16) (256 - TT2PDCNT_A(tt)))

#define ALIVE_PRED_COUNT() (TT2PDCNT_A(B_TT) + TT2PDCNT_A(S_TT))
#define DEAD_PRED_COUNT()  (TT2PDCNT_D(B_TT) + TT2PDCNT_D(S_TT))

#define BW_SEARCH_ONES_BETTER()  (ALIVE_PRED_COUNT() < DEAD_PRED_COUNT())
#define BW_SEARCH_ZEROS_BETTER() (ALIVE_PRED_COUNT() > DEAD_PRED_COUNT())

typedef struct {
	u64 size; // number of states.
	Matx8 states[];
} StateBuffer;

// 2d 8-bit point
typedef struct {
	u8 x, y;
} Point8;

constexpr u8 spread4_lut[16] = {
	0b00000000, 0b00000001, 0b00000100, 0b00000101,
	0b00010000, 0b00010001, 0b00010100, 0b00010101,
	0b01000000, 0b01000001, 0b01000100, 0b01000101,
	0b01010000, 0b01010001, 0b01010100, 0b01010101,
};

static FORCE_INLINE u16 spread8(const u8 x) {
	// double the bit indices for each bit in an 8-bit integer, returning a 16-bit integer
	return spread4_lut[x >> 4] << 8 | spread4_lut[x & 15];
}

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
	while (high > 0) {
		if (high <= 24) {
			for (i64 step = 1; step <= high; step++) {
				u64 key = arr[step];
				i64 j   = (i64) (step - 1);

				while (j >= 0 && key < arr[j]) {
					arr[j + 1] = arr[j];
					j--;
				}

				arr[j + 1] = key;
			}

			return;
		}

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

			return;
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
		i64 i = 1, j = high;

		while (true) {
			while (i <= high && arr[i] < p) i++;
			while (arr[j] > p) j--;

			if (i >= j) break;

			u64 tmp = arr[i];
			arr[i]  = arr[j];
			arr[j]  = tmp;
		}

		arr[0] = arr[j];
		arr[j] = p;

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
}

static FORCE_INLINE void sort(u64 *const arr, const u64 size) {
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

	u8 indices[8] = {0, 1, 2, 3, 4, 5, 6, 7};

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
		.matx = x0.matx | x1.matx | x2.matx |
				x3.matx | x4.matx | x5.matx |
				x6.matx | x7.matx | x8.matx
	};
#elif NEIGHBORHOOD == NH_VON_NEUMANN
	;              nbuf[0] = x1;            ;
	;nbuf[1] = x3;              nbuf[2] = x5;
	;              nbuf[3] = x7;            ;

	const Matx8 bits = {
		.matx =           x1.matx |
				x3.matx | x4.matx | x5.matx |
				          x7.matx
	};
#else // NH_DIAGONAL
	;nbuf[0] = x0;              nbuf[1] = x2;
	;                                       ;
	;nbuf[2] = x6;              nbuf[3] = x8;

	const Matx8 bits = {
		.matx = x0.matx |           x2.matx |
				          x4.matx |
				x6.matx |           x8.matx
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

static u64 advance_ptlstate(Matx8 **const pbuf, const u64 old_cnt, const u64 new_cnt, const u8 bit_idx) {
	// buf = [allocation size] [bitmask] [actual output array]
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

	Matx8 *buf = *pbuf;
	u64 buf_size = buf[-2].matx; // allocation size in bytes

	// the values here should be set previously in states_for_bit
	Matx8 *new = (Matx8 *) hashtable.scratch + 1;

	const u64
		new_bitmask    = new[-1].matx, // bitmask for only the new bit
		shared_bitmask = buf[-1].matx & new_bitmask;

	buf[-1].matx |= new_bitmask;

	// point the temporary buffer right after the old buffer
	Matx8 *tmp = buf + old_cnt;

	u64 idx = 0; // output index

	for (u64 i = 0; i < old_cnt; i++)
	for (u64 j = 0; j < new_cnt; j++)
		// for any bits that both states claim to know, they give the same values
		if ((shared_bitmask & (buf[i].matx ^ new[j].matx)) == 0) {
			if ((idx + old_cnt + 3llu) * sizeof(Matx8) >= buf_size) {
				buf_size <<= 1; // double the size on each overflow
				buf[-2].matx = buf_size;

			#if DEBUG
				const bool ge1GiB = buf_size >= 1024*1024*1024;
				if (!cfg.quiet) enprintf(
					"doubling buffer size to %llu %ciB. running cnt=%llu, current cnt=%llu",
					buf_size >> 20 + 10*ge1GiB, ge1GiB ? 'G' : 'M', old_cnt, idx
				);
			#endif

				buf = realloc(buf - 2, buf_size);
				OOM(buf, 1);

			#if DEBUG
				// sometimes the reallocation can take a long time.
				if (!cfg.quiet)
					// I know fputs is faster, but that is a whole extra DLL import.
					fprintf(stderr, ". done\n");
			#endif

				buf  += 2;
				tmp   = buf + old_cnt;
				*pbuf = buf;
			}

			tmp[idx++].matx = buf[i].matx | new[j].matx;
		}

	const u64 size = idx;

	memmove(buf, tmp, size * sizeof(Matx8));
	return size;
}

static const StateBuffer *_find_predecessors1m(Matx8 state) {
	// returns the number of predecessors found.
	// the elements are in the program scratch buffer
	// tmp - 2 = [allocation size] [partial state count] [partial states...]
	static Matx8 *tmp = NULL;

	if (tmp == NULL) {
		// start off with 4 MiB
		tmp = malloc(4 * 1024 * 1024);
		OOM(tmp, 2);

		tmp += 2;
		tmp[-2].matx = 4 * 1024 * 1024;
	}

	// NOTE: for the diagonal neighborhood, this is likely not the best pattern,
	//       but I don't feel like making a whole separate implementation just
	//       for that, so I am going to pretend that this is just as optimal.

	// temporary buffer for the current bit's states
	tmp[-1].matx = 0; // no bits known
	tmp[ 0].matx = 0; // blank state
	u64 cnt      = 1; // 1 object in the array

	// identity transformation
	Point8 roll = {0};
	u8 best_tfm = TFM_IDENTITY;

	if (BW_SEARCH_ONES_BETTER() || BW_SEARCH_ZEROS_BETTER()) {
		// default to the worst value before finding better ones
		u64 best_val = BW_SEARCH_ONES_BETTER() ? 0 : UINT64_MAX;

		for (u8 tfm = 0; tfm < sizeof(tfm_strs) / sizeof(*tfm_strs); tfm++) {
			const Matx8 tfm_state = Matx8_tfm(state, tfm);

			for (u8 y = 0; y < 8; y++) {
				const Matx8 yroll_state = Matx8_yroll(tfm_state, y);

				for (u8 x = 0; x < 8; x++) {
					const Matx8 xroll_state = Matx8_xroll(yroll_state, x);
					// NOTE: rbit(a) > rbit(b) and a < b are not equivalent
					// row 1: A B C D E F G H
					// row 0: I J K L M N O P
					// rbit =>
					//        H G F E D C B A
					//        P O N M L K J I
					// spread and offset =>
					//        0 H 0 G 0 F 0 E 0 D 0 C 0 B 0 A
					//        P 0 O 0 N 0 M 0 L 0 K 0 J 0 I 0
					// combine =>
					//        P H O G N F M E L D K C J B I A
					// tack on the rest of the rows in reverse order as well.
					// if ones are better, maximize this. otherwise minimize it.
					const u64 val =
						(u64) spread8(rbit8(xroll_state.rows[0])) << 49 |
						(u64) spread8(rbit8(xroll_state.rows[1])) << 48 |
						(u64) rbit8(xroll_state.rows[2]) << 40 |
						(u64) rbit8(xroll_state.rows[3]) << 32 |
						(u64) rbit8(xroll_state.rows[4]) << 24 |
						(u64) rbit8(xroll_state.rows[5]) << 16 |
						(u64) rbit8(xroll_state.rows[6]) <<  8 |
						(u64) rbit8(xroll_state.rows[7]);

					if (BW_SEARCH_ONES_BETTER() ? val > best_val : val < best_val) {
						roll = (Point8) {x, y};
						best_tfm = tfm;
						best_val = val;
					}
				} // foreach x roll
			} // foreach y roll
		} // foreach tfm

		state = Matx8_tfm(state, best_tfm);
		state = Matx8_yroll(state, roll.y);
		state = Matx8_xroll(state, roll.x);

		if (!cfg.quiet)
			printf("transformation = { op = \"%s (%u)\", roll = (%u, %u) }\n",
				tfm_strs[best_tfm], best_tfm, roll.x, roll.y
			);
	} // if (1's better or 0's better)

	u8 bit_idx = 0;

	// form as many 2x2 boxes as possible as early as possible (reduce the search space)
	for (u8 i = 0; i < 8; i++) {
		cnt = advance_ptlstate(
			&tmp, cnt,
			states_for_bit(i, (state.rows[0] >> i) & 1),
			bit_idx++
		);

		if (cnt == 0)
			goto done;

		cnt = advance_ptlstate(
			&tmp, cnt,
			states_for_bit(i + 8, (state.rows[1] >> i) & 1),
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

		// turn the transformation into the inverse transformation.
		if      (best_tfm == TFM_ROT90 ) best_tfm = TFM_ROT270;
		else if (best_tfm == TFM_ROT270) best_tfm = TFM_ROT90;

		if (roll.x > 7 || roll.y > 7 || best_tfm >= sizeof(tfm_strs) / sizeof(*tfm_strs))
			__builtin_unreachable();

		if (roll.x != 0)
			for (u64 i = 0; i < cnt; i++)
				tmp[i] = Matx8_xroll(tmp[i], roll.x);

		if (roll.y != 0)
			for (u64 i = 0; i < cnt; i++)
				tmp[i] = Matx8_yroll(tmp[i], roll.y);

		if (best_tfm != TFM_IDENTITY)
			for (u64 i = 0; i < cnt; i++)
				tmp[i] = Matx8_tfm(tmp[i], best_tfm);
	}

	// point to the element count
	tmp[-1].matx = cnt;
	return (StateBuffer *) (tmp - 1);
}

static FORCE_INLINE const StateBuffer *_find_predecessors1u(u64 state) {
	return _find_predecessors1m((Matx8) {.matx = state});
}

static const StateBuffer *_find_predecessors1sb(const StateBuffer *const prev) {
	// returns a list of all the predecessors of all the given states
	// frees the old buffer
	StateBuffer *running = (StateBuffer *) malloc(sizeof(StateBuffer));
	running->size = 0;

	for (u64 i = 0; i < prev->size; i++) {
		const StateBuffer *new = _find_predecessors1m(prev->states[i]);

		// append the new states to the running states list
		if (new->size > 0) {
			running = (StateBuffer *) realloc(running, sizeof(StateBuffer) +
				(running->size + new->size) * sizeof(Matx8));
			OOM(running, 4);
			memcpy(running->states + running->size, new->states, new->size * sizeof(Matx8));
			running->size += new->size;
		}

		// if this is the first iteration, this step is redundant.
		if (i != 0)
			running->size = uniq(running->states, running->size);
	}

	free((void *) prev);
	return running;
}

#define find_predecessors(x) _Generic((x),     \
	u64                : _find_predecessors1u, \
	Matx8              : _find_predecessors1m, \
	const StateBuffer *: _find_predecessors1sb \
)(x)
