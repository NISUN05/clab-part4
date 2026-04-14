#include <stdio.h>
#include <assert.h>
#include <getopt.h>
#include <strings.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "memlib.h"
#include "mm-common.h"
#include "mm-implicit.h"

/*
 * Test 1: test set_size_status
 * Verifies that set_size_status stores both size and allocated flag correctly
 * in a single meta_t value.
 */
void
test_set_size_status()
{
	printf("start %s...", __func__);

	meta_t val = 0;
	set_size_status(&val, 64, true);
	assert(val == (64 | 0x1L));

	// overwrite with new values
	set_size_status(&val, 1024, false);
	assert(val == (1024 | 0x0L));

	printf("passed\n");
}


/*
 * Test 2: test get_size
 * Verifies that get_size correctly extracts the chunk size from a meta_t value,
 * ignoring the lowest bit (allocated flag).
 */
void
test_get_size()
{
	printf("start %s...", __func__);

	meta_t val;
	// size=128, allocated
	set_size_status(&val, 128, true);
	assert(get_size(&val) == 128);

	// size=256, free
	set_size_status(&val, 256, false);
	assert(get_size(&val) == 256);

	// large size
	set_size_status(&val, 4096, true);
	assert(get_size(&val) == 4096);

	printf("passed\n");
}

/*
 * Test 3: test get_status
 * Verifies that get_status correctly extracts the allocated bit from a meta_t value.
 */
void
test_get_status()
{
	printf("start %s...", __func__);

	meta_t val;
	set_size_status(&val, 128, true);
	assert(get_status(&val) == true);

	set_size_status(&val, 128, false);
	assert(get_status(&val) == false);

	// verify status is independent of size
	set_size_status(&val, 4096, true);
	assert(get_status(&val) == true);

	printf("passed\n");
}

/*
 * Test 4: test set_status
 * Verifies that set_status changes only the allocated bit
 * without modifying the stored size.
 */
void
test_set_status()
{
	printf("start %s...", __func__);

	meta_t val;
	set_size_status(&val, 256, false);
	assert(get_status(&val) == false);
	assert(get_size(&val) == 256);

	// toggle to allocated -- size must be preserved
	set_status(&val, true);
	assert(get_status(&val) == true);
	assert(get_size(&val) == 256);

	// toggle back to free
	set_status(&val, false);
	assert(get_status(&val) == false);
	assert(get_size(&val) == 256);

	printf("passed\n");
}

/*
 * Test 5: test header2footer
 * Verifies that header2footer returns a pointer to the footer
 * (which is hdr_size bytes before the end of the chunk).
 * Also checks that init_chunk sets the footer with the same size and status.
 */
void
test_header2footer()
{
	printf("start %s...", __func__);
	mem_reset_brk();
	mm_init();

	meta_t *h = (meta_t *)mem_sbrk(128);
	init_chunk(h, 128, false);

	meta_t *f = header2footer(h);
	// footer should be at (char *)h + chunk_size - hdr_size
	assert(f == (meta_t *)((char *)h + 128 - hdr_size));
	// footer should store same size and status as header
	assert(get_size(f) == 128);
	assert(get_status(f) == false);

	// test with an allocated chunk of different size
	mem_reset_brk();
	mm_init();
	h = (meta_t *)mem_sbrk(512);
	init_chunk(h, 512, true);
	f = header2footer(h);
	assert(f == (meta_t *)((char *)h + 512 - hdr_size));
	assert(get_size(f) == 512);
	assert(get_status(f) == true);

	printf("passed\n");
}

/*
 * Test 6: test payload2header
 * Verifies that payload2header returns the chunk's header
 * given a pointer to the chunk's payload.
 * The payload starts at header + hdr_size.
 */
void
test_payload2header()
{
	printf("start %s...", __func__);
	mem_reset_brk();
	mm_init();

	meta_t *h = (meta_t *)mem_sbrk(128);
	init_chunk(h, 128, true);
	char *p = (char *)h + hdr_size;
	assert(payload2header(p) == h);

	printf("passed\n");
}

/*
 * Test 7: test next_chunk_header
 * Verifies that next_chunk_header returns the header of the following chunk,
 * or NULL for the last chunk.
 */
void
test_next_chunk_header()
{
	printf("start %s...", __func__);
	mem_reset_brk();
	mm_init();

	// make a test heap of n chunks
    int n = 3;
	meta_t *p[n];
	for (int i = 0; i < n; i++) {
        size_t sz = 256*(i+1);
		p[i] = (meta_t *)mem_sbrk(sz);
		init_chunk(p[i], sz, true);
	}

	for (int i = 0; i < n; i++) {
        if (i < n-1)
            assert(next_chunk_header(p[i]) == p[i+1]);
        else
            assert(next_chunk_header(p[i]) == NULL);
    }

	printf("passed\n");
}

/*
 * Test 8: test prev_chunk_header
 * Verifies that prev_chunk_header returns the header of the preceding chunk
 * by reading the footer of the previous chunk to determine its size.
 */
void
test_prev_chunk_header()
{
	printf("start %s...", __func__);
	mem_reset_brk();
	mm_init();

	// make a test heap of n chunks
    int n = 3;
	meta_t *p[n];
	for (int i = 0; i < n; i++) {
        size_t sz = 256*(i+1);
		p[i] = (meta_t *)mem_sbrk(sz);
		init_chunk(p[i], sz, true);
	}
    
    for (int i = n-1; i >= 0; i--) {
        if (i > 0) 
            assert(prev_chunk_header(p[i]) == p[i-1]);
        else
             assert(prev_chunk_header(p[i]) == NULL);
    }

    printf("passed\n");
}

/*
 * Test 9: test first_chunk_header
 * Verifies that first_chunk_header returns a pointer to the first chunk
 * on the heap, or NULL if the heap has no chunks (only the alignment padding).
 */
void
test_first_chunk_header()
{
	printf("start %s...", __func__);

	// empty heap -- only ALIGNMENT/2 padding from mm_init, no real chunks
	mem_reset_brk();
	mm_init();
	assert(first_chunk_header() == NULL);

	// add a chunk -- first_chunk_header should return it
	meta_t *expected = (meta_t *)mem_sbrk(256);
	init_chunk(expected, 256, true);
	assert(first_chunk_header() == expected);

	printf("passed\n");
}

/*
 * Test 10: test ask_os_for_chunk
 * Verifies that ask_os_for_chunk invokes mem_sbrk for the requested size
 * and initializes the returned chunk as free with correct header/footer.
 */
void
test_ask_os_for_chunk()
{
	printf("start %s...", __func__);
	mem_reset_brk();
	mm_init();

	meta_t *p = ask_os_for_chunk(256);
	assert(get_size(p) == 256);
	assert(get_status(p) == false);
	// heap should contain ALIGNMENT/2 padding + 256 byte chunk
	assert(mem_heapsize() == 256 + ALIGNMENT/2);

	// verify footer is also set correctly
	meta_t *f = header2footer(p);
	assert(get_size(f) == 256);
	assert(get_status(f) == false);

	printf("passed\n");
}

/*
 * Test 11: test mm_checkheap
 * Verifies that mm_checkheap correctly counts allocated/free chunks
 * and sums their sizes.
 */
void
test_mm_checkheap()
{
	printf("start %s...", __func__);
	mem_reset_brk();
	mm_init();

	// make a test heap of n chunks
    int n = 4;
    int free_size = 0;
    int total_size = 0;
    for (int i = 0; i < n; i++) { 
        size_t sz = 256*(i+1);
        total_size += sz;
        if (i % 2) {
            init_chunk(mem_sbrk(sz), sz, true);
        }else {
            free_size += sz;
            init_chunk(mem_sbrk(sz), sz, false);
        }
    }

	heap_info_t info;
	info = mm_checkheap(false);
	assert(info.num_allocated_chunks == 2);
	assert(info.num_free_chunks == 2);
	assert(info.allocated_size == (total_size-free_size));
	assert(info.free_size == free_size);

	printf("passed\n");
}

/*
 * Test 12: test split
 * Verifies that split divides a free chunk into two pieces:
 * the first of the requested size and the second holding the remainder.
 * Both pieces should have correct headers and footers.
 */
void
test_split()
{
	printf("start %s...", __func__);
	mem_reset_brk();
	mm_init();

	// allocate a 256-byte chunk on the heap
	meta_t *h = (meta_t *)mem_sbrk(256);
	init_chunk(h, 256, false);

	// split it into a 64-byte piece and a 192-byte piece
	split(h, 64);

	// verify first piece
	assert(get_size(h) == 64);
	assert(get_status(h) == false);
	assert(get_size(header2footer(h)) == 64);
	assert(get_status(header2footer(h)) == false);

	// verify second piece
	meta_t *next = (meta_t *)((char *)h + 64);
	assert(get_size(next) == 192);
	assert(get_size(header2footer(next)) == 192);
	assert(get_status(next) == false);
	assert(get_status(header2footer(next)) == false);

	// mark first piece as allocated for checkheap test
	set_status(h, true);
	set_status(header2footer(h), true);

    // split the second piece, but remaining is not enough for split
    split(next, 176);
    assert(get_size(next) == 192);
	assert(get_size(header2footer(next)) == 192);
	assert(get_status(next) == false);
	assert(get_status(header2footer(next)) == false);


	heap_info_t info = mm_checkheap(false);
	assert(info.num_allocated_chunks == 1);
	assert(info.num_free_chunks == 1);
	assert(info.allocated_size == 64);
	assert(info.free_size == 192);

	printf("passed\n");
}

/*
 * Test 13: test first_fit
 * Verifies that first_fit returns the first free chunk large enough
 * for the requested size, or NULL when no suitable chunk exists.
 */
void
test_first_fit()
{
	printf("start %s...", __func__);
	mem_reset_brk();
	mm_init();

	// make a test heap:
	// 1st chunk is 32 bytes and free
	init_chunk((meta_t *)mem_sbrk(32), 32, false);
	// 2nd chunk is 64 bytes and allocated 
	init_chunk((meta_t *)mem_sbrk(64), 64, true);
	// 3rd chunk is 128 bytes and free
	meta_t *fh = (meta_t *)mem_sbrk(128);
	init_chunk(fh, 128, false);
	// 4th chunk is 256 bytes and allocated
	init_chunk((meta_t *)mem_sbrk(256), 256, true);

	meta_t *h;
	h = first_fit(64);
	assert(h == fh);

	// on an empty heap, first_fit should return NULL
	mem_reset_brk();
	mm_init();
	h = first_fit(64);
	assert(h == NULL);

	printf("passed\n");
}

/*
 * Test 14: test mm_malloc
 * Verifies that mm_malloc allocates from a free chunk via first-fit,
 * splits the chunk, and returns a properly aligned payload pointer.
 */
void
test_mm_malloc()
{
	printf("start %s...", __func__);
	mem_reset_brk();
	mm_init();

	// make a test heap:
	// 1st chunk is 256 bytes and allocated
	init_chunk((meta_t *)mem_sbrk(256), 256, true);
	// 2nd chunk is 256 bytes and free
	meta_t *fp;
	fp = (meta_t *)mem_sbrk(256);
	init_chunk(fp, 256, false);

	// check that allocation is done in the beginning of
	// the 2nd (free) chunk
	char *p = mm_malloc(60);
	assert(is_aligned(p));
	assert(p == (char *)fp + hdr_size);

	heap_info_t info = mm_checkheap(false);
	assert(info.num_free_chunks == 1);
	assert(info.num_allocated_chunks == 2);

	// malloc again, check that payload is properly aligned
	p = mm_malloc(60);
	assert(is_aligned(p));

	info = mm_checkheap(false);
	assert(info.num_free_chunks == 1);
	assert(info.num_allocated_chunks == 3);
	assert((info.free_size + info.allocated_size) == 512);

	printf("passed\n");
}

/*
 * Test 15: test coalesce_next
 * Verifies that coalesce_next merges a free chunk with its immediately
 * following free chunk, updating both the header and footer.
 * Does NOT merge when the next chunk is allocated.
 */
void
test_coalesce_next()
{
	printf("start %s...", __func__);

	// Case 1: next chunk is free -- should merge
	mem_reset_brk();
	mm_init();
	meta_t *h1 = (meta_t *)mem_sbrk(256);
	init_chunk(h1, 256, false);
	meta_t *h2 = (meta_t *)mem_sbrk(256);
	init_chunk(h2, 256, false);

	meta_t *h = coalesce_next(h1);
    assert(h == h1);
	assert(get_size(h1) == 512);
	assert(get_status(h1) == false);
	// verify footer of merged chunk is updated
	meta_t *f = header2footer(h1);
	assert(get_size(f) == 512);
	assert(get_status(f) == false);

	heap_info_t info = mm_checkheap(false);
	assert(info.num_free_chunks == 1);
	assert(info.free_size == 512);

	// Case 2: next chunk is allocated -- should NOT merge
	mem_reset_brk();
	mm_init();
	h1 = (meta_t *)mem_sbrk(256);
	init_chunk(h1, 256, false);
	h2 = (meta_t *)mem_sbrk(256);
	init_chunk(h2, 256, true);

	h = coalesce_next(h1);
    assert(h == h1);
	assert(get_size(h1) == 256); // unchanged
	assert(get_status(h1) == false); // unchanged

	info = mm_checkheap(false);
	assert(info.num_free_chunks == 1);
	assert(info.free_size == 256);
	assert(info.num_allocated_chunks == 1);

	printf("passed\n");
}

/*
 * Test 16: test coalesce_prev
 * Verifies that coalesce_prev merges a free chunk with its immediately
 * preceding free chunk, updating both the header and footer.
 * Does NOT merge when the previous chunk is allocated.
 */
void
test_coalesce_prev()
{
	printf("start %s...", __func__);

	// Case 1: previous chunk is free -- should merge
	mem_reset_brk();
	mm_init();
	meta_t *h1 = (meta_t *)mem_sbrk(256);
	init_chunk(h1, 256, false);
	meta_t *h2 = (meta_t *)mem_sbrk(256);
	init_chunk(h2, 256, false);

	meta_t *merged = coalesce_prev(h2);
	// should return h1 (the merged chunk starts at h1)
	assert(merged == h1);
	assert(get_size(h1) == 512);
	assert(get_status(h1) == false);
	// verify footer of merged chunk is updated
	meta_t *f = header2footer(h1);
	assert(get_size(f) == 512);
	assert(get_status(f) == false);

	heap_info_t info = mm_checkheap(false);
	assert(info.num_free_chunks == 1);
	assert(info.free_size == 512);

	// Case 2: previous chunk is allocated -- should NOT merge
	mem_reset_brk();
	mm_init();
	h1 = (meta_t *)mem_sbrk(256);
	init_chunk(h1, 256, true);
	h2 = (meta_t *)mem_sbrk(256);
	init_chunk(h2, 256, false);

	merged = coalesce_prev(h2);
    assert(merged == h2);
	assert(get_size(h2) == 256); // unchanged
	assert(get_status(h2) == false); // unchanged

	info = mm_checkheap(false);
	assert(info.num_free_chunks == 1);
	assert(info.free_size == 256);
	assert(info.num_allocated_chunks == 1);

	printf("passed\n");
}

/*
 * Test 17: test mm_free
 * Verifies that mm_free marks a chunk as free and coalesces with
 * both the next and previous free neighbors.
 */
void
test_mm_free()
{
	printf("start %s...", __func__);
	mem_reset_brk();
	mm_init();

	// make a test heap of 3 allocated chunks
	meta_t *h[3];
	for (int i = 0; i < 3; i++) {
		h[i] = (meta_t *)mem_sbrk(256);
		init_chunk(h[i], 256, true);
	}

	// free the middle chunk -- neighbors are allocated, no coalescing
	char *p;
	p = (char *)h[1] + hdr_size;
	mm_free(p);
	heap_info_t info = mm_checkheap(false);
	assert(info.num_free_chunks == 1);
	assert(info.num_allocated_chunks == 2);
	assert(info.free_size == 256);

	// free the last chunk -- should coalesce with prev (h[1] is now free)
	p = (char *)h[2] + hdr_size;
	mm_free(p);
	info = mm_checkheap(false);
	assert(info.num_free_chunks == 1);
	assert(info.num_allocated_chunks == 1);
	assert(info.free_size == 256*2);

	// free the first chunk -- should coalesce with next (h[1]+h[2] merged)
	p = (char *)h[0] + hdr_size;
	mm_free(p);
	// all three chunks should be merged into one
	info = mm_checkheap(false);
	assert(info.num_free_chunks == 1);
	assert(info.num_allocated_chunks == 0);
	assert(info.free_size == 256*3);

	printf("passed\n");
}

/*
 * Test 18: test mm_realloc
 * Verifies that mm_realloc can grow an allocation in-place by coalescing
 * with the next free chunk, avoiding an unnecessary copy.
 */
void
test_mm_realloc()
{
	printf("start %s...", __func__);
	mem_reset_brk();
	mm_init();

	// make a test heap of 3 chunks:
	// 1st is free, 2nd is allocated, 3rd is free.
	meta_t *h[3];
	for (int i = 0; i < 3; i++) {
		h[i] = (meta_t *)mem_sbrk(256);
		init_chunk(h[i], 256, i==1? true: false);
	}

	heap_info_t info = mm_checkheap(false);
	assert(info.num_free_chunks == 2);
	assert(info.num_allocated_chunks == 1);

	// realloc the 2nd chunk to a size that requires merging with
	// the next (3rd) free chunk.
	// new chunk size required align(480)+2*hdr_size = 480+16 = 496
	char *p = mm_realloc((char *)h[1] + hdr_size, 480);
	// should allocate in-place after coalescing with next chunk
	assert(p == (char *)h[1] + hdr_size);
    assert(get_size(h[1])==512);
    assert(get_status(h[1])==true);

	info = mm_checkheap(false);
	assert(info.num_allocated_chunks == 1);
	assert(info.num_free_chunks == 1);
	// h[0] should still be free

    mem_reset_brk();
	mm_init();

	// make a test heap of 3 chunks:
	// 1st is free, 2nd is allocated, 3rd is free.
	for (int i = 0; i < 3; i++) {
		h[i] = (meta_t *)mem_sbrk(256);
		init_chunk(h[i], 256, i==1? true: false);
	}
	// realloc the 2nd chunk to a size that requires merging with
	// the next (3rd) free chunk.
	// new chunk size required align(395)+2*hdr_size = 400+16 = 416
	p = mm_realloc((char *)h[1] + hdr_size, 395);
	// should allocate in-place after coalescing with next chunk
	assert(p == (char *)h[1] + hdr_size);
    assert(get_size(h[1])==416);
    assert(get_status(h[1])==true);

    info = mm_checkheap(false);
	assert(info.num_allocated_chunks == 1);
	assert(info.num_free_chunks == 2);

	printf("passed\n");
}

typedef void (*TestFunc)();

int
main(int argc, char **argv)
{
#define NUM_TESTS 18
	TestFunc fs[NUM_TESTS] = {
		test_set_size_status,     /*  1 */
		test_get_size,            /*  2 */
		test_get_status,          /*  3 */
		test_set_status,          /*  4 */
		test_header2footer,       /*  5 */
		test_payload2header,      /*  6 */
		test_next_chunk_header,   /*  7 */
		test_prev_chunk_header,   /*  8 */
		test_first_chunk_header,  /*  9 */
		test_ask_os_for_chunk,    /* 10 */
		test_mm_checkheap,        /* 11 */
		test_split,               /* 12 */
		test_first_fit,           /* 13 */
		test_mm_malloc,           /* 14 */
		test_coalesce_next,       /* 15 */
		test_coalesce_prev,       /* 16 */
		test_mm_free,             /* 17 */
		test_mm_realloc           /* 18 */
	};

	mem_init();

	int which_test = 0;
	int c;
	while ((c = getopt(argc, argv, "t:")) != -1) {
		switch (c) {
			case 't':
				which_test = atoi(optarg);
				break;
		}
	}
	for (int i = 0; i < NUM_TESTS; i++) {
		if ((which_test == 0) || (which_test == (i+1))) {
			fs[i]();
			if (which_test > 0)
				break;
		}
	}
}
