// Implicit free list malloc implementation
// Each chunk has a header and footer (of type meta_t)
//
// Key optimizations over naive first-fit:
//   1. NEXT-FIT search  – a roving pointer avoids re-scanning allocated
//      blocks at the front of the heap on every malloc call.
//   2. In-place realloc – tries to satisfy realloc without copying when
//      the current block (or current + next free block) is large enough.
//
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
// Next-fit rover  (the heart of the throughput improvement)
// ---------------------------------------------------------------------------
// Instead of always starting the free-chunk search at the very first chunk,
// we remember where the last search left off.  On the next malloc call we
// continue from that point, wrapping around to the beginning only once if
// needed.  This turns the amortised search cost from O(heap) to O(gap)
// between consecutive allocations, which is far cheaper on real workloads.
static meta_t *rover = NULL;   // NULL means "start from the beginning"

// ---------------------------------------------------------------------------
// Basic header/footer helpers
// ---------------------------------------------------------------------------

void
set_size_status(meta_t *h, size_t csz, bool allocated)
{
    h->size_status = csz | (allocated ? 1 : 0);
}

size_t
get_size(meta_t *h)
{
    return h->size_status & ~((size_t)1);
}

bool
get_status(meta_t *h)
{
    return (h->size_status & 1) == 1;
}

void
set_status(meta_t *h, bool allocated)
{
    if (allocated) {
        h->size_status |= (size_t)1;
    } else {
        h->size_status &= ~((size_t)1);
    }
}

// ---------------------------------------------------------------------------
// Pointer arithmetic helpers
// ---------------------------------------------------------------------------

meta_t *
header2footer(meta_t *h)
{
    size_t csz = get_size(h);
    return (meta_t *)((char *)h + csz - hdr_size);
}

meta_t *
payload2header(void *p)
{
    return (meta_t *)((char *)p - hdr_size);
}

// Returns the next chunk's header, or NULL if this is the last chunk.
// mem_heap_hi() points to the LAST VALID BYTE, so the test is ">".
meta_t *
next_chunk_header(meta_t *h)
{
    size_t csz = get_size(h);
    meta_t *next = (meta_t *)((char *)h + csz);
    if ((void *)next > mem_heap_hi()) {
        return NULL;
    }
    return next;
}

meta_t *
prev_chunk_header(meta_t *h)
{
    meta_t *first = first_chunk_header();
    if (h == first) {
        return NULL;
    }
    meta_t *prev_footer = (meta_t *)((char *)h - hdr_size);
    size_t prev_csz = get_size(prev_footer);
    return (meta_t *)((char *)h - prev_csz);
}

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

void
init_chunk(meta_t *p, size_t csz, bool allocated)
{
    set_size_status(p, csz, allocated);
    meta_t *footer = header2footer(p);
    set_size_status(footer, csz, allocated);
}

meta_t *
ask_os_for_chunk(size_t csz)
{
    assert(csz % ALIGNMENT == 0);
    void *p = mem_sbrk(csz);
    assert((((unsigned long)p) % ALIGNMENT) == ALIGNMENT / 2);
    init_chunk(p, csz, false);
    return (meta_t *)p;
}

// ---------------------------------------------------------------------------
// mm_init  – also resets the rover so each trace starts clean
// ---------------------------------------------------------------------------
int
mm_init(void)
{
    assert(2 * sizeof(meta_t) == ALIGNMENT);
    mem_sbrk(sizeof(meta_t));   // 8-byte alignment padding
    rover = NULL;               // reset rover for each new trace
    return 0;
}

// ---------------------------------------------------------------------------
// mm_checkheap  (thorough)
// ---------------------------------------------------------------------------
heap_info_t
mm_checkheap(bool verbose)
{
    heap_info_t info = {0};

    meta_t *h = first_chunk_header();
    bool prev_was_free = false;

    while (h != NULL) {
        size_t csz   = get_size(h);
        bool   status = get_status(h);

        // Invariant 1: positive, ALIGNMENT-multiple size
        assert(csz > 0 && "chunk size must be positive");
        assert(csz % ALIGNMENT == 0 && "chunk size must be ALIGNMENT-aligned");

        // Invariant 2: header and footer match
        meta_t *footer = header2footer(h);
        assert(footer->size_status == h->size_status && "header/footer mismatch");

        // Invariant 3: no two consecutive free chunks
        assert(!(prev_was_free && !status) && "two consecutive free chunks");

        // Invariant 4: both header and footer within heap
        assert((void *)h      >= mem_heap_lo() && "header below heap base");
        assert((void *)footer <= mem_heap_hi() && "footer above heap top");

        // Invariant 5: minimum chunk size
        assert(csz >= 2 * hdr_size && "chunk smaller than minimum");

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

    // Invariant 6: chunk sizes account for full heap
    assert(mem_heapsize() - ALIGNMENT / 2
           == info.allocated_size + info.free_size
           && "chunk sizes do not account for full heap");

    return info;
}

// ---------------------------------------------------------------------------
// next_fit  –  replaces first_fit
// ---------------------------------------------------------------------------
// Start searching from 'rover'. If we reach the end without finding a fit,
// wrap around and search from the beginning up to (but not including) rover.
// This keeps the search short by skipping the dense region of recently-freed
// small blocks that pile up at the front of the heap under first-fit.
meta_t *
first_fit(size_t csz)
{
    // If the heap is empty or rover is stale, fall back to the beginning.
    meta_t *start = (rover != NULL) ? rover : first_chunk_header();
    if (start == NULL) {
        return NULL;
    }

    // Phase 1: search from rover to end of heap
    meta_t *h = start;
    while (h != NULL) {
        if (!get_status(h) && get_size(h) >= csz) {
            rover = h;   // remember where we stopped
            return h;
        }
        h = next_chunk_header(h);
    }

    // Phase 2: wrap around – search from the beginning up to start
    h = first_chunk_header();
    while (h != NULL && h != start) {
        if (!get_status(h) && get_size(h) >= csz) {
            rover = h;
            return h;
        }
        h = next_chunk_header(h);
    }

    return NULL;   // no fit found anywhere
}

// ---------------------------------------------------------------------------
// split
// ---------------------------------------------------------------------------
void
split(meta_t *original, size_t csz)
{
    size_t orig_csz   = get_size(original);
    size_t remainder  = orig_csz - csz;

    // Only split if the remainder can hold header + footer + aligned payload.
    if (remainder < 2 * hdr_size || remainder % ALIGNMENT != 0) {
        return;
    }

    bool orig_status = get_status(original);
    init_chunk(original, csz, orig_status);

    meta_t *new_chunk = (meta_t *)((char *)original + csz);
    init_chunk(new_chunk, remainder, false);

    // If the rover was pointing at the original chunk, advance it to the
    // newly created free remainder so the next search starts right there.
    if (rover == original) {
        rover = new_chunk;
    }
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

    size_t payload = align(size);
    size_t csz     = 2 * hdr_size + payload;

    meta_t *p = first_fit(csz);

    if (p != NULL) {
        split(p, csz);
        set_status(p, true);
        set_status(header2footer(p), true);
    } else {
        // Grow the heap.  The new chunk comes right at the current brk,
        // so advance rover past it after allocation.
        p = ask_os_for_chunk(csz);
        set_status(p, true);
        set_status(header2footer(p), true);
        rover = NULL;   // heap grew; next search starts fresh
    }

    if (debug) {
        mm_checkheap(true);
    }

    return (void *)((char *)p + hdr_size);
}

// ---------------------------------------------------------------------------
// Coalescing helpers
// ---------------------------------------------------------------------------

meta_t *
coalesce_next(meta_t *h)
{
    meta_t *next = next_chunk_header(h);
    if (next == NULL || get_status(next)) {
        return h;
    }
    // If rover pointed at the next chunk, pull it back to h.
    if (rover == next) {
        rover = h;
    }
    size_t new_csz = get_size(h) + get_size(next);
    init_chunk(h, new_csz, false);
    return h;
}

meta_t *
coalesce_prev(meta_t *h)
{
    meta_t *prev = prev_chunk_header(h);
    if (prev == NULL || get_status(prev)) {
        return h;
    }
    // If rover pointed at h, move it back to prev.
    if (rover == h) {
        rover = prev;
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

    set_status(h, false);
    set_status(header2footer(h), false);

    h = coalesce_next(h);
    h = coalesce_prev(h);

    // Point rover at the freshly freed (and possibly merged) chunk so the
    // next malloc call can reuse it immediately without scanning past it.
    rover = h;

    if (debug) {
        mm_checkheap(true);
    }
}

// ---------------------------------------------------------------------------
// mm_realloc  (optimised: in-place resize avoids copies when possible)
// ---------------------------------------------------------------------------
void *
mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL) {
        return mm_malloc(size);
    }
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    meta_t *old_hdr = payload2header(ptr);
    size_t  old_csz = get_size(old_hdr);
    size_t  new_pay = align(size);
    size_t  new_csz = 2 * hdr_size + new_pay;

    // ------------------------------------------------------------------
    // Case 1: current block is already large enough – split off excess.
    // ------------------------------------------------------------------
    if (old_csz >= new_csz) {
        split(old_hdr, new_csz);
        if (debug) mm_checkheap(true);
        return ptr;
    }

    // ------------------------------------------------------------------
    // Case 2: absorb the immediately following free block in-place.
    // ------------------------------------------------------------------
    meta_t *next = next_chunk_header(old_hdr);
    if (next != NULL && !get_status(next)) {
        size_t combined = old_csz + get_size(next);
        if (combined >= new_csz) {
            // Pull rover away from next before we absorb it.
            if (rover == next) {
                rover = old_hdr;
            }
            init_chunk(old_hdr, combined, true);
            split(old_hdr, new_csz);
            set_status(old_hdr, true);
            set_status(header2footer(old_hdr), true);
            if (debug) mm_checkheap(true);
            return ptr;
        }
    }

    // ------------------------------------------------------------------
    // Case 3: must allocate elsewhere and copy.
    // ------------------------------------------------------------------
    void *new_ptr = mm_malloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }

    size_t old_pay   = old_csz - 2 * hdr_size;
    size_t copy_size = (old_pay < new_pay) ? old_pay : new_pay;
    memmove(new_ptr, ptr, copy_size);

    mm_free(ptr);

    if (debug) mm_checkheap(true);
    return new_ptr;
}


