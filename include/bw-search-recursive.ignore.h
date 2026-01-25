#pragma once
#define BW_SEARCH_H
// this file is not intended to actually be used. It is just here for records.
// this algorithm is magnitudes slower than the other one.

// backwards search

#ifndef B_TT
	#pragma message("B_TT not defined. assuming B2")
	#define B_TT 0b000000100u
#endif

#ifndef S_TT
	#pragma message("S_TT not defined. assuming S23")
	#define S_TT 0b000001100u
#endif

typedef struct {
	u64 size; // number of states.
	u64 bitmask;
	Matx8 states[];
} PtlState;

typedef struct {
	u64 alloc_size;
	PtlState list;
} StateBuffer;

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

static void _gen_combos(
	PtlState *const restrict buf,
	const Matx8 *const restrict arr,
	const Matx8 center,
	const u8 n,
	const u8 k
) {
	// writes all k-combinations of partial states given the array of bits.

	// assumes buf has space for least (n choose k) elements
	// returns a pointer to the next write location.
	// n is the total number of neighbors
	// each element in `arr`, should be something like `1llu << bit`
	// NOTE: `buf` must point to somewhere in the program scratch buffer

	if (n > 8 || k > 8 || k > n)
		return;

	// if (!cfg.quiet)
	// 	printf("    _gen_combos(center=%u, n=%u, k=%u)\n", !!center.matx, n, k);

	if (k == 0) {
		// these will happen first, so don't worry about buffer overflows.
		// if you compile it with that small of a buffer, that is your fault
		buf->states[buf->size++] = center;
		return;
	}

	// k is never more than 8, so just allocate 8 bytes.
	u8 indices[8] = {0, 1, 2, 3, 4, 5, 6, 7};
	u64 idx       = buf->size;

	while (true) {
		// combine the states together and output the state
		if (idx >= SCRATCH_SIZE / sizeof(Matx8) - 3llu) {
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

			buf->states[idx++] = e;
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

	buf->size = idx;
}

static void states_for_bit(Matx8 **const pbuf, u64 offset, const u8 bit, const bool alive) {
	// writes the temporary states to the scratch buffer
	// does not add the bitmask to the start of the array
	// returns the number of objects it created
	// `alive` is the value of the current cell in the next state

	// this doesn't need to use the scratch buffer, but I don't feel like
	// implementing extra buffer over flow control.

	// x0 x1 x2
	// x3 x4 x5
	// x6 x7 x8
	constexpr u8 max_neighbors = 4 << (NEIGHBORHOOD == NH_MOORE);
	PtlState *tmp = (PtlState *) hashtable.scratch;
	Matx8 nbuf[max_neighbors]; // neighbor buffer

	const Matx8 x4 = {.matx = 1llu << bit};

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

	tmp->size = 0;

	for (u8 neighbors = 0; neighbors <= max_neighbors; neighbors++) {
		// the XORs are there because the birth and survive numbers are for if
		// the next state's cell is alive. if it isn't, then the bits need to be flipped.

		// cases where it used to be dead (e.g. center == 0)
		if (((B_TT >> neighbors) & 1) ^ !alive)
			_gen_combos(tmp, nbuf, (Matx8) {0}, max_neighbors, neighbors);

		// cases where it used to be alive (e.g. center != 0)
		if (((S_TT >> neighbors) & 1) ^ !alive)
			_gen_combos(tmp, nbuf, x4, max_neighbors, neighbors);
	}

	Matx8 *buf = *pbuf;
	PtlState *A = (PtlState *) (buf + offset);

	A->size    = tmp->size;
	A->bitmask = bits.matx;

	if ((offset + tmp->size) * sizeof(Matx8) > buf->matx) {
		u64 old_size = buf->matx;
		buf->matx = __builtin_stdc_bit_ceil(offset + tmp->size) * sizeof(Matx8);
		if (!cfg.quiet)
			enprintf("resizing buffer from %llu to %llu KiB (loc 1)\n", old_size >> 10, buf->matx >> 10);

		buf = realloc(buf, buf->matx);
		OOM(buf, 1);
		A = (PtlState *) (buf + offset);
		*pbuf = buf;
	}

	// memmove is not required because A is on the heap and tmp is in BSS
	memcpy(A->states, tmp->states, tmp->size * sizeof(Matx8));
}

/*
 63 62 61 60 | 59 58 57 56
 55 54 53 52 | 51 50 49 48
 47 46 45 44 | 43 42 41 40
 39 38 37 36 | 35 34 33 32
-------------+-------------
 31 30 29 28 | 27 26 25 24
 23 22 21 20 | 19 18 17 16
 15 14 13 12 | 11 10  9  8
  7  6  5  4 |  3  2  1  0
*/

static void merge_ptlstates(Matx8 **const pbuf, const u64 A_ofs, const u64 B_ofs, const u8 bit_idx) {
	// buf = [allocation size] [bitmask] [data]
	// NOTE: if old_cnt == 0, this will not do anything.
	// returns the new value of old_cnt

#if DEBUG
	// TODO: get rid of this at some point.
	// A_ofs < B_ofs should always hold.
	if (A_ofs > B_ofs) {
		eprintf("error: A_ofs > B_ofs (%llu, %llu)\n", A_ofs, B_ofs);
		exit(67);
	}
	if (A_ofs == B_ofs) {
		eprintf("error: A_ofs == B_ofs (%llu, %llu)\n", A_ofs, B_ofs);
		exit(67);
	}
#endif

	Matx8 *buf  = *pbuf;
	PtlState *A = (PtlState *) (buf + A_ofs);
	PtlState *B = (PtlState *) (buf + B_ofs);

	const u64 A_cnt = A->size;
	const u64 B_cnt = B->size;

	if (!cfg.quiet) {
		printf("merge_ptlstates(A={size=%llu, ofs=%llu", A_cnt, A_ofs);

		likely_if (A_ofs != B_ofs)
			printf("}, B={size=%llu, ofs=%llu", B_cnt, B_ofs);

		printf("}, bit_idx=%u)\n", bit_idx);
	}

	unlikely_if (A_cnt == 0 && bit_idx == 0) {
		// TODO: figure out if this actually does anything.
		//       depending on how I implement it, this might not do anything.

		// for the first bit, if there are no predecessors, just copy the new states over.
		A->size    = B->size;
		A->bitmask = B->bitmask;

		memmove(A->states, B->states, B->size * sizeof(Matx8));
		return;
	}

	unlikely_if (A_cnt == 0 || B_cnt == 0 || bit_idx == 64)
		// cartesian product of something with an empty set is an empty set.
		// also, bit index 64 is a sentinel value that means print the data and exit.
		return;

	// point the temporary buffer right after the second input buffer
	const u64 tmp_ofs = B_ofs + sizeof(PtlState) / sizeof(u64) + B_cnt;

	u64 buf_size = buf->matx; // allocation size in bytes
	const u64 shared_bitmask = A->bitmask & B->bitmask;

	A->bitmask |= B->bitmask;

	PtlState *tmp = (PtlState *) (buf + tmp_ofs);

	u64 idx = 0; // buffer index

	for (u64 i = 0; i < A_cnt; i++)
	for (u64 j = 0; j < B_cnt; j++)
		// for any bits that both states claim to know, they give the same values
		if ((shared_bitmask & (A->states[i].matx ^ B->states[j].matx)) == 0) {
			if ((tmp_ofs + idx) * sizeof(Matx8) + sizeof(PtlState) > buf_size) {
				// double the size on each overflow
				buf_size = __builtin_stdc_bit_ceil(tmp_ofs + idx) * sizeof(Matx8);
				if (!cfg.quiet)
					enprintf("resizing buffer from %llu to %llu KiB (loc 2)\n", buf->matx >> 10, buf_size >> 10);
				buf->matx = buf_size;

				buf = realloc(buf, buf_size);
				OOM(buf, 2);

				A   = (PtlState *) (buf +   A_ofs);
				B   = (PtlState *) (buf +   B_ofs);
				tmp = (PtlState *) (buf + tmp_ofs);

				*pbuf = buf;
			}

			tmp->states[idx].matx = A->states[i].matx | B->states[j].matx;
			idx++;
		}

	A->size = idx;

	memmove(A->states, tmp->states, idx * sizeof(Matx8));
}

#define PtlStateAt(buf, offset) ((PtlState *) (buf + offset))
#define PtlState_get(buf, offset, attr) ( ((PtlState *) (buf + offset))->attr )

static u64 recursive_bwsearch(
	Matx8 **const pbuf,
	const Matx8 state,
	u64 offset,
	u8 height,
	u8 width,
	const u8 origin // index of the lowest bit in the group
) {
	// buf = [allocation size] [PtlState list 1] [PtlState list 2] ...
	// returns the offset to the most recent object.
	// next write addr: `offset + 2 + ((PtlState *) (buffer + offset))->size`

	if (!cfg.quiet)
		printf("recursive_bwsearch(ofs=%llu, \"%ux%u\", origin=\"bit %u\")\n",
			offset, height, width, origin);

	Matx8 *const buf = *pbuf;

	if (width > 8 || height > 8)
		__builtin_unreachable();

	u64 A_ofs, B_ofs;

	if (width > height) {
		// width == 2*height
		width >>= 1;
		
		A_ofs   = recursive_bwsearch(pbuf, state, offset, height, width, origin);
		offset += PtlState_get(buf, A_ofs, size) + sizeof(PtlState) / sizeof(u64);
		B_ofs   = recursive_bwsearch(pbuf, state, offset, height, width, origin + width);
		merge_ptlstates(pbuf, A_ofs, B_ofs, origin);
		return A_ofs;
	}

	if (width == 1 && height == 1) {
		states_for_bit(pbuf, offset, origin, (state.matx >> origin) & 1);
		return offset;
	}

	// width == height && width != 1

	height >>= 1;
	A_ofs   = recursive_bwsearch(pbuf, state, offset, height, width, origin);
	offset += PtlStateAt(buf, A_ofs)->size + sizeof(PtlState) / sizeof(u64);
	B_ofs   = recursive_bwsearch(pbuf, state, offset, height, width, origin + (width << 2));
	merge_ptlstates(pbuf, A_ofs, B_ofs, origin);
	return A_ofs;
}

static FORCE_INLINE StateBuffer *find_predecessors(const Matx8 state) {
	// NOTE: this returns a pointer to this functions internal buffer,
	//       so if you call this multiple times, you must move the data
	//       to a different buffer.

	// NOTE: `StateBuffer` implies there is only a single list,
	//       but there are actually up to 7 lists at a time.
	static Matx8 *buf = NULL;

	if (buf == NULL) {
		// start off with 4 MiB
		buf = malloc(256 * 1024 * 1024);
		OOM(buf, 3);

		buf->matx = 256 * 1024 * 1024;
	}

	recursive_bwsearch(
		&buf,
		state,
		1, // offset
		8, // height
		8, // width
		0  // origin bit
	);

	PtlState *const A = (PtlState *) (buf + 1);
	A->size = uniq(A->states, A->size);

	// print the final count information.
	merge_ptlstates(
		&buf,
		1, // offset A
		0, // offset B
		64 // bit index
	);

	return (StateBuffer *) buf;
}

#if 0
static FORCE_INLINE Matx8 *find_predecessors_old(Matx8 state) {
	// returns the number of predecessors found.
	// the elements are in the program scratch buffer
	static Matx8 *tmp = NULL;

	if (tmp == NULL) {
		// start off with 4 MiB
		tmp = malloc(4 * 1024 * 1024);
		OOM(tmp, 4);

		tmp[0].matx = 4 * 1024 * 1024;
	}

	u8 bit_idx = 0;

	u64 A_ofs = 1;
	u64 B_ofs = 1;

	// form as many 2x2 boxes as possible as early as possible (reduce the search space)
	for (u8 i = 0; i < 8; i++) {
		u64 A_cnt;

		states_for_bit(&tmp, B_ofs, i, (state.rows[0] >> i) & 1);
		merge_ptlstates(&tmp, A_ofs, B_ofs, bit_idx++);
		A_cnt = ((PtlState *) (tmp + A_ofs))->size;
		B_ofs = A_ofs + 2 + A_cnt;

		if (A_cnt == 0)
			goto done;

		states_for_bit(&tmp, B_ofs, i, (state.rows[1] >> i) & 1);
		merge_ptlstates(&tmp, A_ofs, B_ofs, bit_idx++);
		A_cnt = ((PtlState *) (tmp + A_ofs))->size;
		B_ofs = A_ofs + 2 + A_cnt;

		if (A_cnt == 0)
			goto done;
	}

	// starting here, almost all cells form a 2x2 box just by iterating forwards.
	for (u8 i = 16; i < 64 && A_cnt > 0; i++) {
		states_for_bit(&tmp, B_ofs, i, (state.matx >> i) & 1);
		merge_ptlstates(&tmp, A_ofs, B_ofs, bit_idx++);
		A_cnt = ((PtlState *) (tmp + A_ofs))->size;
		B_ofs = A_ofs + 2 + A_cnt;
	}

	// transfer the data from `tmp` to `out`
	A_cnt = uniq(((PtlState *) (tmp + A_ofs))->states, A_cnt);
done:
	// print the final object count
	merge_ptlstates(NULL /-*unneeded argument*/, cnt, 0, 64);

	// void states_for_bit(Matx8 **pbuf, u64 offset, u8 bit, bool alive);
	// PtlState *merge_ptlstates(Matx8 **pbuf, u64 A_ofs, u64 B_ofs, u8 bit_idx)

	Matx8 *out = (Matx8 *) malloc((cnt + 1) * sizeof(Matx8)) + 1;
	OOM(out, 5);

	out[-1].matx = cnt;
	memcpy(out, tmp, cnt * sizeof(Matx8));

	return out;
}
#endif

static FORCE_INLINE StateBuffer *advance_predecessors(const StateBuffer *const prev_buf) {
	StateBuffer *running_buf  = (StateBuffer *) malloc(sizeof(StateBuffer));
	running_buf->alloc_size   = sizeof(StateBuffer);
	running_buf->list.size    = 0;
	running_buf->list.bitmask = ~0llu; // not required

	for (u64 i = 0; i < prev_buf->list.size; i++) {
		const StateBuffer *new_buf = find_predecessors(prev_buf->list.states[i]);

		// append the new states to the running states list
		const u64 new_cnt = new_buf->list.size;
		if (new_cnt > 0) {
			// reallocate and update allocation metadata
			running_buf->alloc_size += new_cnt * sizeof(Matx8);
			running_buf = (StateBuffer *) realloc(running_buf, running_buf->alloc_size);
			OOM(running_buf, 6);

			// append the elements to the end of the list
			memcpy(
				running_buf->list.states + running_buf->list.size,
				new_buf->list.states,
				new_cnt * sizeof(Matx8)
			);

			// update the list metadata
			running_buf->list.size += new_buf->list.size;
		}

		free((void *) new_buf);
	}

	running_buf->list.size = uniq(running_buf->list.states, running_buf->list.size);
	free((void *) prev_buf);
	return running_buf;
}
