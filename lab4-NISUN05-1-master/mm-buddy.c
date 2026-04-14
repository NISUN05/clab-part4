// This file gives you a starting point to implement malloc using a buddy system
//
// Unlike the implicit-list design, the buddy system does NOT use a footer.
// Each chunk has only a header_t storing its size class (log2 of chunk size)
// and an allocated flag. Free chunks overlay a free_header_t at the same
// address, adding prev/next pointers for per-size-class doubly-linked free lists.
//
// See mm-buddy.h for a detailed design overview.

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include "mm-common.h"
#include "mm-buddy.h"
#include "memlib.h"

// Turn debug on while testing correctness; off when measuring performance.
static bool debug = true;

// One doubly-linked free list per size class (index i → size class i+MINSZ).
free_header_t *flists[NLISTS];

// Heap base address, set by mm_init and used by get_buddy.
void *init_mem_lo;

// Remove node n from the doubly-linked list headed by *head.
void
list_remove(free_header_t **head, free_header_t *n)
{
    //Your code here
}

// Prepend node n to the doubly-linked list headed by *head.
void
list_insert(free_header_t **head, free_header_t *n)
{
    //Your code here
}

// Return the buddy of chunk h at the given size_class, or NULL if the
// buddy address falls outside the current heap bounds.
header_t *
get_buddy(header_t *h, int size_class)
{
    //Your code here
}

// Given a payload pointer, return a pointer to the chunk's header.
// The header immediately precedes the payload.
header_t *
payload2header(void *p)
{
    //Your code here
}

// mm_init is called once before the first malloc/free/realloc call.
// It zeroes all free lists and records the heap base address.
// Its implementation is given below
int
mm_init()
{
    // Sanity-check: the minimum chunk (2^MINSZ bytes) must be large enough
    // to hold a free_header_t and must be ALIGNMENT-aligned.
    assert((sizeof(header_t) % ALIGNMENT) == 0);
    assert((sizeof(free_header_t) % ALIGNMENT) == 0);
    assert((1 << MINSZ) >= sizeof(free_header_t));

    for (int i = 0; i < NLISTS; i++) {
        flists[i] = NULL;
    }

    // Record the heap base so get_buddy can compute XOR offsets.
    // No padding is needed: the first block will naturally start at init_mem_lo.
    init_mem_lo = mem_heap_lo();
    return 0;
}

// Traverse all heap blocks from init_mem_lo and verify:
//   1. Every block has a valid size_class in [MINSZ, MINSZ+NLISTS).
//   2. Every block is naturally aligned: its byte offset from init_mem_lo
//      is a multiple of its size (1 << size_class).
//   3. No two free buddies of the same size_class coexist (buddy invariant).
//   4. The free lists exactly match the set of free blocks in the heap.
// Returns a heap_info_t with aggregate counts and byte totals.
heap_info_t
mm_checkheap(bool verbose)
{
    heap_info_t info;
    //Your code here 
    return info;
}

// Ask the OS for a free block of at least the given size_class.
// Because mem_sbrk returns consecutive addresses, the alignment of each new
// block depends on the current heap size. This function computes how large a
// naturally aligned block can be obtained at the current heap top, requests it,
// and repeats (coalescing partial results) until it has a block of the
// requested size_class or larger.
free_header_t *
ask_os_for_block(size_t size_class)
{
    //Your code here
}

// Compute the size class required to fit a payload of the given size.
//
// The total bytes needed for one chunk = size + sizeof(header_t).
// The returned size_class k is the number of times you must right-shift
// (size + sizeof(header_t)) until it reaches 0.
// Equivalently, 1 << k is the smallest power of 2 that can accommodate
// size + sizeof(header_t) bytes (with one extra doubling for exact powers).
//
// Note: size_class must always be >= MINSZ so the chunk can hold a free_header_t.
size_t
get_size_class(size_t size)
{
    // Your code here
}

// Recursively split free block h until it has exactly the requested size_class,
// allocating the block and returning a pointer to its header.
//
// Base case: h->header.size_class == size_class
//   Mark h as allocated and return &h->header.
//
// Recursive case: h->header.size_class > size_class
//   1. Decrement h->header.size_class by 1.
//   2. Find the buddy using get_buddy(&h->header, h->header.size_class).
//      (The buddy is always at a higher address after decrement -- assert this.)
//   3. Initialize the buddy as a free block with the same (new) size_class.
//   4. Insert the buddy into flists[buddy->header.size_class - MINSZ].
//   5. Recurse: return split_n_alloc(h, size_class).
//
// Precondition: h is not allocated, h->header.size_class >= size_class.
header_t *
split_n_alloc(free_header_t *h, int size_class)
{
    // Your code here
}

// Try to merge free block *fh with its buddy (in either direction).
//
// Returns true if the merge succeeded (caller should call again to keep
// coalescing); false if the buddy cannot be merged. Store the pointer 
// to the merged block in *fh
// Steps:
//   1. Compute the buddy using get_buddy((header_t *)*fh, (*fh)->header.size_class).
//   2. If the buddy does not exist, is allocated, or has a different
//      size_class than *fh, return false.
//   3. Remove the buddy from flists[buddy->size_class - MINSZ].
//   4. If buddy < *fh, store it so *fh points to the lower-addressed block.
//   5. Increment (*fh)->header.size_class (the merged block is one class larger).
//   6. Return true.
bool
coalesce(free_header_t **fh)
{
    // Your code here
}

// Allocate a block of the given payload size and return a pointer to the payload.
//
// Logic:
//   1. Compute the required size_class using get_size_class(size).
//   2. Search flists[size_class-MINSZ] through flists[NLISTS-1] for the first
//      non-empty free list:
//        a. Remove the head block from that free list.
//        b. Split and allocate it: call split_n_alloc(h, size_class).
//   3. If no free list had a block, ask the OS: call ask_os_for_block(size_class),
//      then mark the result as allocated (set .size_class and .allocated directly,
//      since ask_os_for_block returns a block that is not yet allocated).
//   4. Compute and return the payload pointer: (char *)allocated_header + sizeof(header_t).
void *
mm_malloc(size_t size)
{
    // Your code here

    // After allocating, check heap correctness to catch bugs early.
    if (debug) {
        mm_checkheap(false);
    }
    return NULL;
}

// Free a previously allocated block and coalesce with buddies.
//
// Logic:
//   1. Get the block header: payload2header(p), cast to free_header_t *.
//   2. Mark the block as not allocated: fh->header.allocated = false.
//   3. Repeatedly call coalesce(&fh) until it returns false,
//      merging with free buddies as much as possible (either direction).
//   4. Insert the final (possibly merged) block into
//      flists[fh->header.size_class - MINSZ].
void
mm_free(void *p)
{
    // Your code here

    // After freeing, check heap correctness to catch bugs early.
    if (debug) {
        mm_checkheap(false);
    }
}

// Resize a previously allocated block.
//
// Special cases:
//   - If p is NULL, behave like mm_malloc(size).
//   - If size is 0 and p is not NULL, behave like mm_free(p) and return NULL.
void *
mm_realloc(void *p, size_t size)
{
    // Your code here

    // After reallocating, check heap correctness to catch bugs early.
    if (debug) {
        mm_checkheap(false);
    }
    return NULL;
}
