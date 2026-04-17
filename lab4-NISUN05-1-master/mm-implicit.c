// Implicit free list malloc implementation
// Each chunk has a header and footer (of type meta_t)
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm-common.h"
#include "mm-implicit.h"
#include "memlib.h"

// Set debug = false for performance measurement.
// Set to true only when debugging correctness.
static bool debug = false;

// hdr_size is the header (or footer) size
size_t hdr_size = sizeof(meta_t);

// ---------------------------------------------------------------------------
// Basic header/footer helpers
// ---------------------------------------------------------------------------

// Set both the size and status bit into the word pointed to by h.
// The status bit is packed into the LSB (chunk sizes are always even).
void
set_size_status(meta_t *h, size_t csz, bool allocated)
{
    h->size_status = csz | (allocated ? 1 : 0);
}

// Extract chunk size (mask off the LSB status bit).
size_t
get_size(meta_t *h)
{
    return h->size_status & ~((size_t)1);
}

// Extract allocation status from the LSB.
bool
get_status(meta_t *h)
{
    return (h->size_status & 1) == 1;
}

// Flip only the status bit, leaving the size unchanged.
void
set_status(meta_t *h, bool allocated)
{
    if (allocated) {
        h->size_status |= (size_t)1;      // set LSB
    } else {
        h->size_status &= ~((size_t)1);   // clear LSB
    }
}

// ---------------------------------------------------------------------------
// Pointer arithmetic helpers
// ---------------------------------------------------------------------------

// Given a pointer to a chunk header, return a pointer to its footer.
meta_t *
header2footer(meta_t *h)
{
    size_t csz = get_size(h);
    return (meta_t *)((char *)h + csz - hdr_size);
}

// Given a pointer to a chunk's payload, return a pointer to its header.
meta_t *
payload2header(void *p)
{
    return (meta_t *)((char *)p - hdr_size);
}

// Given a pointer to this chunk's header, return a pointer to the NEXT
// chunk's header.  Returns NULL if this is the last chunk on the heap.
// NOTE: mem_heap_hi() points to the LAST VALID BYTE, so the test is ">".
meta_t *
next_chunk_header(meta_t *h)
{
    size_t csz = get_size(h);
    meta_t *next = (meta_t *)((char *)h + csz);
    if ((void *)next > mem_heap_hi()) {   // Bug fix: was ">=" (off-by-one)
        return NULL;
    }
    return next;
}

// Given a pointer to this chunk's header, return a pointer to the PREVIOUS
// chunk's header by reading its footer.  Returns NULL for the first chunk.
meta_t *
prev_chunk_header(meta_t *h)
{
    meta_t *first = first_chunk_header();
    if (h == first) {
        return NULL;
    }
    // The previous chunk's footer sits immediately before this header.
    meta_t *prev_footer = (meta_t *)((char *)h - hdr_size);
    size_t prev_csz = get_size(prev_footer);
    return (meta_t *)((char *)h - prev_csz);
}

// Return a pointer to the first chunk on the heap, or NULL if the heap
// is empty.  The first chunk starts at ALIGNMENT/2 bytes from the heap base
// so that its payload is ALIGNMENT-aligned.
meta_t *
first_chunk_header()
{
    void *h = (void *)mem_heap_lo() + ALIGNMENT / 2;
    if (h >= mem_heap_hi()) {
        return NULL;
    }
    return (meta_t *)h;
}

// ---------------------------------------------------------------------------
// Chunk initialisation / OS interaction
// ---------------------------------------------------------------------------

// Initialise (or reinitialise) a chunk: write identical header and footer.
void
init_chunk(meta_t *p, size_t csz, bool allocated)
{
    set_size_status(p, csz, allocated);
    meta_t *footer = header2footer(p);
    set_size_status(footer, csz, allocated);
}

// Ask the OS for a new chunk of exactly csz bytes (must be ALIGNMENT-aligned),
// initialise it as free, and return its header pointer.
meta_t *
ask_os_for_chunk(size_t csz)
{
    assert(csz % ALIGNMENT == 0);
    void *p = mem_sbrk(csz);
    // mem_sbrk returns a pointer at ALIGNMENT/2 offset from an alignment boundary
    assert((((unsigned long)p) % ALIGNMENT) == ALIGNMENT / 2);
    init_chunk(p, csz, false);
    return (meta_t *)p;
}

// ---------------------------------------------------------------------------
// mm_init
// ---------------------------------------------------------------------------

// Initialise the allocator.  Place an 8-byte padding word at the base of
// the heap so the first real chunk's payload is 16-byte aligned.
int
mm_init(void)
{
    assert(2 * sizeof(meta_t) == ALIGNMENT);
    mem_sbrk(sizeof(meta_t));   // 8-byte alignment padding
    return 0;
}

// ---------------------------------------------------------------------------
// mm_checkheap  (thorough version)
// ---------------------------------------------------------------------------

heap_info_t
mm_checkheap(bool verbose)
{
    heap_info_t info = {0};

    meta_t *h = first_chunk_header();
    bool prev_was_free = false;   // used to check the no-consecutive-free invariant

    while (h != NULL) {
        size_t csz = get_size(h);
        bool   status = get_status(h);

        // ------------------------------------------------------------------
        // Invariant 1: every chunk has a positive, ALIGNMENT-multiple size.
        // ------------------------------------------------------------------
        assert(csz > 0 && "chunk size must be positive");
        assert(csz % ALIGNMENT == 0 && "chunk size must be ALIGNMENT-aligned");

        // ------------------------------------------------------------------
        // Invariant 2: header and footer must match.
        // ------------------------------------------------------------------
        meta_t *footer = header2footer(h);
        assert(footer->size_status == h->size_status
               && "header/footer mismatch");

        // ------------------------------------------------------------------
        // Invariant 3: no two consecutive free chunks (coalescing must have
        //              merged them already).
        // ------------------------------------------------------------------
        assert(!(prev_was_free && !status)
               && "two consecutive free chunks – coalescing failed");

        // ------------------------------------------------------------------
        // Invariant 4: both header and footer lie within the heap.
        // ------------------------------------------------------------------
        assert((void *)h      >= mem_heap_lo() && "header below heap base");
        assert((void *)footer <= mem_heap_hi() && "footer above heap top");

        // ------------------------------------------------------------------
        // Invariant 5: minimum chunk size (header + footer = 2*hdr_size).
        // ------------------------------------------------------------------
        assert(csz >= 2 * hdr_size && "chunk smaller than minimum");

        // Accumulate statistics.
        if (status) {
            info.allocated_size += csz;
            info.allocated_cnt++;
        } else {
            info.free_size += csz;
            info.free_cnt++;
        }

        if (verbose) {
            printf("  chunk @ %p: size=%zu, %s\n",
                   (void *)h, csz, status ? "allocated" : "free");
        }

        prev_was_free = !status;
        h = next_chunk_header(h);
    }

    // ------------------------------------------------------------------
    // Invariant 6: sum of all chunk sizes equals heap size minus padding.
    // ------------------------------------------------------------------
    assert(mem_heapsize() - ALIGNMENT / 2
           == info.allocated_size + info.free_size
           && "chunk sizes do not account for full heap");

    return info;
}

// ---------------------------------------------------------------------------
// first_fit / split
// ---------------------------------------------------------------------------

// Traverse the heap from the beginning and return the first free chunk
// whose size is >= csz, or NULL if none exists.
meta_t *
first_fit(size_t csz)
{
    meta_t *h = first_chunk_header();
    while (h != NULL) {
        if (!get_status(h) && get_size(h) >= csz) {
            return h;
        }
        h = next_chunk_header(h);
    }
    return NULL;
}

// Split the chunk pointed to by 'original' into a chunk of size csz
// (which retains original's allocation status) and a free remainder chunk.
// The split only happens if the remainder is large enough to be a valid
// standalone chunk (>= 2*hdr_size and ALIGNMENT-aligned).
void
split(meta_t *original, size_t csz)
{
    size_t orig_csz = get_size(original);
    size_t remainder = orig_csz - csz;

    // Do not split if the remainder cannot hold even a header+footer pair,
    // or if it is not properly aligned (would corrupt the heap layout).
    if (remainder < 2 * hdr_size || remainder % ALIGNMENT != 0) {
        return;
    }

    bool orig_status = get_status(original);
    init_chunk(original, csz, orig_status);                        // shrink original

    meta_t *new_chunk = (meta_t *)((char *)original + csz);
    init_chunk(new_chunk, remainder, false);                       // free remainder
}

// ---------------------------------------------------------------------------
// mm_malloc
// ---------------------------------------------------------------------------

void *
mm_malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }

    // Align the requested payload size, then add header + footer overhead.
    size_t payload = align(size);
    size_t csz     = 2 * hdr_size + payload;

    meta_t *p = first_fit(csz);

    if (p != NULL) {
        // Reuse an existing free chunk; split off any excess.
        split(p, csz);
        set_status(p, true);
        set_status(header2footer(p), true);
    } else {
        // No suitable free chunk – grow the heap.
        p = ask_os_for_chunk(csz);
        set_status(p, true);
        set_status(header2footer(p), true);
    }

    if (debug) {
        mm_checkheap(true);
    }

    // Return pointer to the payload (just past the header).
    return (void *)((char *)p + hdr_size);
}

// ---------------------------------------------------------------------------
// Coalescing helpers
// ---------------------------------------------------------------------------

// Merge this chunk with its immediate successor if that successor is free.
// Returns the (possibly unchanged) header of the merged chunk.
meta_t *
coalesce_next(meta_t *h)
{
    meta_t *next = next_chunk_header(h);
    if (next == NULL || get_status(next)) {
        return h;   // nothing to merge
    }
    size_t new_csz = get_size(h) + get_size(next);
    init_chunk(h, new_csz, false);
    return h;
}

// Merge this chunk with its immediate predecessor if that predecessor is free.
// Returns the header of the merged chunk (which may be the predecessor's header).
meta_t *
coalesce_prev(meta_t *h)
{
    meta_t *prev = prev_chunk_header(h);
    if (prev == NULL || get_status(prev)) {
        return h;   // nothing to merge
    }
    size_t new_csz = get_size(prev) + get_size(h);
    init_chunk(prev, new_csz, false);
    return prev;
}

// ---------------------------------------------------------------------------
// mm_free
// ---------------------------------------------------------------------------

void
mm_free(void *p)
{
    if (p == NULL) {
        return;
    }

    meta_t *h = payload2header(p);

    // Mark the chunk free in both header and footer.
    set_status(h, false);
    set_status(header2footer(h), false);

    // Coalesce with neighbours to prevent fragmentation.
    h = coalesce_next(h);   // merge with following chunk first
    h = coalesce_prev(h);   // then with preceding chunk
    (void)h;

    if (debug) {
        mm_checkheap(true);
    }
}

// ---------------------------------------------------------------------------
// mm_realloc  (optimised: avoids unnecessary copy when possible)
// ---------------------------------------------------------------------------

void *
mm_realloc(void *ptr, size_t size)
{
    // Edge cases required by the spec.
    if (ptr == NULL) {
        return mm_malloc(size);
    }
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    meta_t *old_hdr  = payload2header(ptr);
    size_t  old_csz  = get_size(old_hdr);
    size_t  new_pay  = align(size);
    size_t  new_csz  = 2 * hdr_size + new_pay;

    // ------------------------------------------------------------------
    // Case 1: The current chunk is already large enough.
    //         Try to split off any surplus as a new free chunk.
    // ------------------------------------------------------------------
    if (old_csz >= new_csz) {
        split(old_hdr, new_csz);
        // After split, old_hdr is still allocated with size new_csz (or old_csz
        // if the remainder was too small to split).
        if (debug) {
            mm_checkheap(true);
        }
        return ptr;
    }

    // ------------------------------------------------------------------
    // Case 2: The next chunk is free and together they are large enough.
    //         Absorb the next chunk in-place, avoiding any data copy.
    // ------------------------------------------------------------------
    meta_t *next = next_chunk_header(old_hdr);
    if (next != NULL && !get_status(next)) {
        size_t combined = old_csz + get_size(next);
        if (combined >= new_csz) {
            // Absorb next into this chunk, then split off any excess.
            init_chunk(old_hdr, combined, true);
            split(old_hdr, new_csz);
            // Ensure the (possibly shortened) chunk is marked allocated.
            set_status(old_hdr, true);
            set_status(header2footer(old_hdr), true);
            if (debug) {
                mm_checkheap(true);
            }
            return ptr;
        }
    }

    // ------------------------------------------------------------------
    // Case 3: No in-place option – allocate a new block, copy, then free.
    // ------------------------------------------------------------------
    void *new_ptr = mm_malloc(size);
    if (new_ptr == NULL) {
        return NULL;    // allocation failed; old block is untouched
    }

    // Copy the minimum of old and new payload sizes.
    size_t old_pay   = old_csz - 2 * hdr_size;
    size_t copy_size = old_pay < new_pay ? old_pay : new_pay;
    memmove(new_ptr, ptr, copy_size);

    mm_free(ptr);

    if (debug) {
        mm_checkheap(true);
    }
    return new_ptr;
}



