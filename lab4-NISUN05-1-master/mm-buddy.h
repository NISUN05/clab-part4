#include <stdlib.h>
#include <stdbool.h>

// Every heap chunk has a fixed-size header (header_t) that stores:
//   - size_class: the log2 of the chunk's size in bytes (chunk_bytes = 1 << size_class)
//   - allocated: whether the chunk is in use
//
// Free chunks additionally overlay a free_header_t at the same address,
// which extends header_t with prev/next pointers so free chunks can be
// stored in per-size-class doubly-linked free lists (flists[]).
//
// Chunk sizes are always powers of two, starting at 2^MINSZ bytes.
// 2^MINSZ must be >= sizeof(free_header_t) so that even the smallest
// free chunk can hold its linked-list pointers.
//
// For any chunk at address H with size_class k, its "buddy" is the
// other half of the naturally aligned 2^(k+1)-byte region it belongs to.
// The buddy's address is: init_mem_lo + ((H - init_mem_lo) XOR (1 << k))
//
// When a chunk is freed, it is coalesced with its buddy (if the buddy is
// also free and the same size class), producing a chunk one size class
// larger. This repeats until no further coalescing is possible.
//
// Allocation is served by finding the smallest free list with a block
// large enough, splitting it down to the exact size class needed, and
// marking it allocated.

typedef struct {
    size_t size_class; // log2 of chunk size: chunk is (1 << size_class) bytes
    bool allocated;
} header_t;

typedef struct fheader {
    header_t header;      // must remain first field; free_header_t is cast to/from header_t
    struct fheader *prev;
    struct fheader *next;
} free_header_t;

// Minimum chunk size class. 2^MINSZ == sizeof(free_header_t).
// All chunks are at least this large so free chunks can hold list pointers.
#define MINSZ 5

// Number of segregated free lists.
// flists[i] holds free chunks of size class (i + MINSZ).
// The largest supported chunk is 1 << (MINSZ + NLISTS - 1) bytes.
#define NLISTS 20

// One doubly-linked free list per size class.
// Index i corresponds to size class (i + MINSZ).
extern free_header_t *flists[NLISTS];

// The heap's base address, recorded by mm_init.
// Used by get_buddy to compute XOR-based buddy addresses.
extern void *init_mem_lo;

// Compute the size class needed for a payload of the given size.
// The size class k satisfies: 1 << k is large enough to hold both the
// payload and the header_t overhead.
size_t get_size_class(size_t size);

// Return the buddy of chunk h at the given size_class, or NULL if the
// buddy address falls outside the current heap bounds.
header_t *get_buddy(header_t *h, int size_class);

// Given a payload pointer, return a pointer to the chunk's header.
// The header immediately precedes the payload.
header_t *payload2header(void *p);

// Doubly-linked list operations on per-size-class free lists.
void list_remove(free_header_t **head, free_header_t *n);
void list_insert(free_header_t **head, free_header_t *n);

// Try to coalesce free block *fh with its buddy (in either direction).
// Returns true if the merge succeeded; false if the buddy does not exist,
// is allocated, or has a different size class.
// A pointer to the merged free block is stored in *fh
bool coalesce(free_header_t **fh);

// Recursively split free block h until it reaches exactly size_class,
// inserting split-off buddies into the appropriate free lists.
// Marks the final block allocated and returns a pointer to its header.
header_t *split_n_alloc(free_header_t *h, int size_class);

// Request a free block of at least size_class from the OS,
free_header_t *ask_os_for_block(size_t size_class);

