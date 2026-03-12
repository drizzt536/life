segment .text
	global radix_sort_u64

;; In-place MSB Radix Sort for unsigned 64-bit integer elements using unsigned 64-bit indices
;; x4040 means 0x40-bit elements and 0x40-bit indices.

;; I wrote this in assembly because the fuckass C compiler won't just do what its told.

%ifdifi
;; approximate C version. this won't actually compile to the same code because C likes wasting
;; RAM, but in spirit, it is the same thing. Also I did alter a few things in between writing
;; this C version and doing the assembly, so probably don't just copy/paste this into C code.

u64 *_ip_msb_rdx_sort_x4040(u64 *arr, u64 left, u64 right, u64 byte_pos) {
	if (left >= right || byte_pos > 7)
		return arr;

	byte_pos <<= 3; // change to bit position

	u64 bucket_stt[257];
	{
		// count occurrences of each byte value
		u64 count[256] = {0};
		for (u64 i = left; i <= right; i++)
			count[(arr[i] >> byte_pos) & 0xFF]++;

		// bucket start positions
		for (u64 i = 0; i <= 256; i++) {
			bucket_stt[i] = left;
			left += count[i];
		}
	}

	{
		u64 bucket_cur[256];
		memcpy(bucket_cur, bucket_stt, 256 * sizeof(u64));

		for (u64 bucket = 0; bucket < 256; bucket++) {
			const u64 bucket_end = bucket_stt[bucket + 1];

			if (bucket_stt[bucket] == bucket_end)
				continue;

			u64 i;

			while ((i = bucket_cur[bucket]) < bucket_end) {
				const u8 tgt_bucket = (arr[i] >> byte_pos) & 0xFF;

				if (tgt_bucket != bucket) {
					const u64 j = bucket_cur[tgt_bucket];
					// swap with an element in the target bucket
					const u64 t1 = arr[i];
					const u64 t2 = arr[j];

					arr[i] = t2;
					arr[j] = t1;
				}

				bucket_cur[tgt_bucket]++;
			}
		}
	}

	// GCC doesn't like this next line
	// asm volatile ("add rsp, 2048" : : : "rsp", "cc");

	// don't recurse after byte 0
	if (byte_pos == 0)
		return arr;

	byte_pos >>= 3;
	byte_pos--;

	// recursively sort each bucket by next byte

	for (u64 i = 256; i --> 0 ;) {
		u64 bucket_B = bucket_stt[i + 1];

		if (bucket_B == 0)
			return arr;

		u64 bucket_A = bucket_stt[i];
		bucket_B--;

		// only process buckets with at least 2 elements
		if (bucket_A >= bucket_B)
			continue;

		_ip_msb_rdx_sort_x4040(arr, bucket_A, bucket_B, byte_pos);
	}

	return arr;
}

u64 *radix_sort_u64(u64 *arr, u64 size) {
	return _ip_msb_rdx_sort_x4040(arr, 0, size - 1, 7);
}
%endif

%assign 	nbuckets			256

%xdefine	count				rsp
%assign 	sizeof_count		nbuckets*8

;; bucket_cur is the same memory as count, but for a different purpose
%xdefine	bucket_cur			count
%assign 	sizeof_bucket_cur	sizeof_count

;; bucket_stt comes right after count in memory.
%xdefine	bucket_stt			count + sizeof_count
%assign 	sizeof_bucket_stt	(nbuckets + 1)*8


%xdefine arr rcx

_ip_msb_rdx_sort_x4040.return_early:
	;; this is before the start of the function because the end of the function is more than 127 bytes away
	;; from the jumps, so this lets it do a short jump instead of a near jump
	;; this saves 3 bytes for each jump.
	mov 	rax, arr
	ret

;; u64 *radix_sort_u64(u64 *arr, u64 size);
radix_sort_u64:
	mov 	r9d, 7
	lea 	r8, [rdx - 1]
	xor 	edx, edx
;	mov 	rcx, rcx
;	jmp _ip_msb_rdx_sort_x4040
	;; intentional branch fallthrough

;; arr      => rcx
;; left     => rdx
;; right    => r8
;; byte_pos => r9 (r9b)

;; u64 *_ip_msb_rdx_sort_x4040(u64 *arr, u64 left, u64 right, u64 byte_pos);
_ip_msb_rdx_sort_x4040:
	cmp 	rdx, r8									; if (left >= right)
	jae 	.return_early							;     return arr;
	cmp 	r9b, 7									; if (byte_pos > 7)
	ja  	.return_early							;     return arr;

.prologue:
	;; NOTE: this is a leaf function (kind of, but close enough), so it doesn't need
	;;       the stack to be 16-byte aligned.
	push	rdi
	push	rsi

	;; NOTE: there is no shadow space because this does not put the argument values onto the stack,
	;;       and it does not call anything external that does either, so it doesn't need it.
	;;       Also, it doesn't use AVX or call anything that does, so it doesn't ned 16 byte alignment either.

	;; no ___chkstk_ms: this is called from C so the current page is already committed,
	;; and since I subtract by less than 2 pages, the OS will just allocate the next page
	;; when I touch it (in `rep stosq` at `.count`).
	sub 	rsp, sizeof_bucket_stt + sizeof_count
	;; NOTE: not that it matters, but the stack is aligned properly for libc function calls

	shl 	r9b, 3									; byte_pos <<= 3;
	;; NOTE: byte_pos will now be referred to as bit_pos until the recursion step.

.count:												; u64 count[nbuckets] = {0};
	;; zero `count` memory.

	mov 	rsi, arr	;; save `arr` for later.
%xdefine arr rsi

	xor 	eax, eax	;; zero byte
	mov 	rdi, rsp
	mov 	ecx, nbuckets
rep	stosq

.count@stage_1:
	mov 	rcx, rdx								; u64 i = left;
.count@stage_1@loop: ;; generate bucket sizes
	cmp 	rcx, r8
	ja  	.count@stage_2
	shrx	rax, qword [arr + rcx*8], r9			;     u64 idx = arr[i] >> bit_pos;
	movzx	eax, al									;     idx &= 0xFF;
	inc 	qword [count + rax*8]					;     count[idx]++;
	inc 	rcx										;     i++;
	jmp 	.count@stage_1@loop						; }

	;; NOTE: at this point, right (r8) is no longer needed
.count@stage_2:
	xor 	ecx, ecx								; i = 0;
.count@stage_2@loop: ;; generate bucket start positions
	;; NOTE: the last `left += count[i];` reads past the end of the count array.
	;;       this is okay because `bucket_stt` is always right after it, and it
	;;       is just a read, and `left` is discarded after, so there are no issues.
	cmp 	cx, nbuckets							; while (i <= nbuckets) {
	ja  	.partition
	mov 	qword [bucket_stt + rcx*8], rdx			;     bucket_stt[i] = left;
	add 	rdx, qword [count + rcx*8]				;     left += count[i];
	inc 	ecx										;     i++;
	jmp 	.count@stage_2@loop						; }
	;; NOTE: left (rdx) is no longer needed

	;; NOTE: at this point, the upper 48 bits of rcx are zeroed

.partition:
	;; NOTE: at this point, `count` is no longer needed, and the memory is
	;;       instead used by bucket_cur. bucket_cur == rsp.

	mov 	rax, arr						;; move arr to rax
%xdefine arr rax

	mov 	rdi, bucket_cur					;; arg1 = bucket_cur
	lea 	rsi, [rdi + sizeof_bucket_cur]	;; arg2 = bucket_stt. rdi has a smaller opcode than rsp here.
	mov 	cx, nbuckets					;; arg3 = nbuckets * sizeof(u64)
rep	movsq									;; call memcpy

	;; NOTE: rcx == 0 here
	;; NOTE: at this point, rcx, rdx, r8, r10, r11, rdi, and rsi are free.

	;; arr        => rax
	;; bucket     => rcx (cx)
	;; bucket_end => rdx
	;; i          => r8
	;; bit_pos    => r9
	;; j          => r10
	;; t1         => rsi
	;; t2         => r11
	;; tgt_bucket => rdi

	mov 	cx, nbuckets							; u16 bucket = nbuckets;
.partition@bucket:
	jrcxz	.recurse								; while (bucket != 0) {
	dec 	ecx										;     bucket--;

	mov 	rdx, qword [bucket_stt + (rcx + 1)*8]	;     u64 bucket_end = bucket_stt[bucket + 1];

	cmp 	qword [bucket_stt + rcx*8], rdx			;     if (bucket_stt[bucket] == bucket_end)
	je  	.partition@bucket						;         continue;

	;; NOTE: at this point, r8, r10, r11, rdi, and rsi are free.

.partition@element:
	mov 	r8, qword [bucket_cur + rcx*8]
	cmp 	r8, rdx									;     while ((i = bucket_cur[bucket]) < bucket_end) {
	jae 	.partition@bucket
	shrx	rdi, qword [arr + r8*8], r9				;         u8 tgt_bucket = arr[i] >> byte_pos;
	movzx	rdi, dil								;         tgt_bucket &= 0xFF;

	cmp 	edi, ecx								;         if (tgt_bucket == bucket)
	je  	.partition@element@after				;             goto after;

	mov 	r10, qword [bucket_cur + rdi*8]			;         const u64 j = bucket_cur[tgt_bucket];

	mov 	rsi, qword [arr + r8*8]					;         const u64 t1 = arr[i];
	mov 	r11, qword [arr + r10*8]				;         const u64 t2 = arr[j];
	mov 	qword [arr + r8*8], r11					;         arr[i] = t2;
	mov 	qword [arr + r10*8], rsi				;         arr[j] = t1;
	;; intentional branch fallthrough

.partition@element@after:							;     }
	inc 	qword [bucket_cur + rdi*8]				;     bucket_cur[tgt_bucket]++;
	jmp 	.partition@element						;     continue;

.recurse:											; }
	;; bucket_cur isn't needed anymore, so we can deallocate it from the stack.
	;; this saves 14 KiB of stack space across the 8 recursion levels.
	;; this also has the added benefit of making bucket_stt == rsp, too.

	add 	rsp, sizeof_bucket_cur					; /* no C equivalent operation */

%xdefine bucket_stt rsp

	test	r9b, r9b								; if (bit_pos == 0)
	jz  	.return									;     return arr; // don't recurse after the LSB

	;; now r9 is byte_pos again instead of bit_pos
	shr 	r9b, 3									; byte_pos >>= 3;
	dec 	r9b										; byte_pos--;

	;; NOTE: at this point, rcx, rdx, r8, r10, r11, rdi, and rsi are free

	;; recursively sort each bucket by next byte
	;; arr      => rax
	;; i        => rdi
	;; bucket_A => rdx
	;; bucket_B => r8
	;; byte_pos => rsi

	;; NOTE: arr is stored in rax right now, and we can pretend it is a nonvolatile
	;;       register because the only function this calls is itself, and this
	;;       function returns the array, so rax ends back up as the array pointer.

	mov 	rsi, r9	;; save byte_pos for across recursion calls.
	mov 	di, 256									; u64 i = 256;
.recurse@loop:
	test	edi, edi								; while (i != 0) {
	jz  	.return									;
	dec 	edi										;     i--;
	;; NOTE: use rdx and r8 as the temporary values for bucket_A and bucket_B because
	;;       those are the argument registers the would go in if the recursion happens.

	mov 	r8, qword [bucket_stt + (rdi + 1)*8]	;     const u64 bucket_B = bucket_stt[i + 1];

	test	r8, r8									;     if (bucket_B == 0)
	jz  	.return									;         return arr;

	mov 	rdx, qword [bucket_stt + rdi*8]			;     const u64 bucket_A = bucket_stt[i];
	dec 	r8										;     bucket_B--;

	cmp 	rdx, r8									;     if (bucket_A >= bucket_B)
	jae 	.recurse@loop							;         continue;

	mov 	rcx, arr								;     // set argument 1
	mov 	r9, rsi									;     // set argument 4
	call	_ip_msb_rdx_sort_x4040					;     // recurse
	jmp 	.recurse@loop							; }

.return:
	add 	rsp, sizeof_bucket_stt					; return arr;
	pop 	rsi
	pop 	rdi
	ret
