#pragma once
#define TABLE_H

// defines TableEntry, HashTable, and the Table_* API

#include "matx8.h"

#define TABLE_LEN (1 << TABLE_BITS)

#define TABLE_NO_VALUE  (~0u)
#define ARENA_NO_BUCKET (~0u)
#define ARENA_NO_NEXT  ((~0u) - 1u)

// each entry takes up a quarter of a cache line.
typedef struct {
	Matx8 key; // the matrix. 8 bytes
	u32 val;   // step index the state belongs to.
	u32 next;  // arena id for the next entry in the linked list.
} __attribute__((aligned(16))) TableEntry;

#define SCRATCH_SIZE ((TABLE_LEN + ARENA_LEN)*sizeof(TableEntry) + 1*sizeof(u32))

typedef union {
	// generic byte buffer for storing strings in some parts of the program.
	// this just uses the same memory location as the hash table, but is not part of it.
	char scratch[SCRATCH_SIZE];

	// NOTE: The first layer of entries is directly in the hash table. Usually
	//       the hashmap is an array of pointers, but I like this way better.
	__attribute__((packed)) struct {
		TableEntry buckets[TABLE_LEN];
		TableEntry overflow_arena[ARENA_LEN];
		u32 arena_pos; // next write index
	};
} __attribute__((aligned(64))) HashTable; // aligned to a cache line.

static HashTable hashtable = {0};

#if DEBUG
static u64 total_collisions = 0;
#endif

static bool Table__add3(const Matx8 mat, const u32 val, const u32 h) {
	// returns true if it is out of memory and false otherwise

	if (hashtable.buckets[h].next == ARENA_NO_BUCKET) {
		// no collision.
		hashtable.buckets[h] = (TableEntry) {
			.key  = mat,
			.val  = val,
			.next = ARENA_NO_NEXT
		};
	}
	else {
		// handle collision

	#if DEBUG
		total_collisions++;
	#endif

		if (hashtable.arena_pos == ARENA_MAX)
			return true; // out of arena memory

		const u32 entry_id = hashtable.arena_pos++;

		// NOTE: if there is a collision, the `next` field will never be ARENA_NO_BUCKET,
		//       so don't worry about trying to normalize the `next` field.

		// don't traverse the linked list. just put the new entry at in the second spot.
		hashtable.overflow_arena[entry_id] = (TableEntry) {
			.key  = mat,
			.next = hashtable.buckets[h].next,
			.val  = val
		};

		hashtable.buckets[h].next = entry_id;
	}

	return false;
}

static FORCE_INLINE bool Table__add2(const Matx8 mat, const u32 val) {
	return Table__add3(mat, val, Matx8_hash(mat));
}

static u32 Table__get2(const Matx8 mat, u32 h) {
	TableEntry *entry = hashtable.buckets + h;

	if (entry->next == ARENA_NO_BUCKET)
		// bucket is empty. no value to retrieve.
		return TABLE_NO_VALUE;

	while (true) {
		if (entry->key.matx == mat.matx)
			return entry->val;

		if (entry->next == ARENA_NO_NEXT)
			break;

		entry = hashtable.overflow_arena + entry->next;
	}

	return TABLE_NO_VALUE;
}

static FORCE_INLINE u32 Table__get1(const Matx8 mat) {
	return Table__get2(mat, Matx8_hash(mat));
}

/////////////////////////// public API methods //////////////////////////

// NOTE: returns false if it succeeded, and true if it ran out of arena memory.
#define Table_add(mat, val, h...) \
	VA_IF(Table__add3(mat, val, (h)), Table__add2(mat, val), h)

// NOTE: it returns TABLE_NO_VALUE (~0u) if the entry doesn't exist.
//       a real value should not get that high, so it is fine.
#define Table_get(mat, h...) \
	VA_IF(Table__get2(mat, (h)), Table__get1(mat), h)

static void Table_clear(void) {
	_Static_assert((TABLE_LEN & 3) == 0, "TABLE_LEN must be a multiple of 4");

	hashtable.arena_pos = 0;

	for (u32 i = 0; i < TABLE_LEN; i += 4) {
		// only touch one cache line per iteration.
		hashtable.buckets[i | 0].next = ARENA_NO_BUCKET;
		hashtable.buckets[i | 1].next = ARENA_NO_BUCKET;
		hashtable.buckets[i | 2].next = ARENA_NO_BUCKET;
		hashtable.buckets[i | 3].next = ARENA_NO_BUCKET;

		// NOTE: the other fields don't need to be cleared because they won't
		//       be read again until after the next time they are set.
	}
}
