// This file gives you a starting point to implement malloc using implicit list
// Each chunk has a header and a footer (of type meta_t) 
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "mm-common.h"
#include "mm-implicit.h"
#include "memlib.h"

// turn "debug" on while you are debugging correctness. 
// Turn it off when you want to measure performance
static bool debug = true; 

//hdr_size is the header (or footer) size
size_t hdr_size = sizeof(meta_t);

// helper function to set the header/footer field pointed to by "p" with 
// the given chunk size (csz) and status (allocated) information.
// It embeds the status bit in the least significant bit position 
void 
set_size_status(meta_t *h, size_t csz, bool allocated)
{
    //Your code here
}

// helper function to extract the chunk size information from the 
// header/footer pointed to by p
size_t
get_size(meta_t *h)
{
    //Your code here
}

// helper function to extract the status (allocated or free) information 
// from the header/footer pointed to by p
bool
get_status(meta_t *h)
{
    //Your code here
}

// helper function to set the status information in the header/footer
// while leaving the chunk size information unchanged
void
set_status(meta_t *h, bool allocated)
{
    //Your code here
}

// helper function that returns a pointer to the footer of a chunk 
// given a pointer "h" to the header of this chunk
// To be used by various other functions (e.g. init_chunk, split, coalesce)
meta_t *
header2footer(meta_t *h)
{
    //Your code here
}

// helper function that returns a pointer to this chunk's header 
// when given pointer "p" to the chunk's payload
// To be used by mm_free(...)
meta_t *
payload2header(void *p)
{
    //Your code here
}

// helper function that returns a pointer to 
// the next (i.e. following) chunk's header when given pointer to this chunk's header. 
// If this chunk is the last chunk on the heap (aka the calculated next chunk's header pointer 
// is beyond the heap's high address h >= mem_heap_hi()), returns NULL
// To be used by first_fit(...) and mm_checkheap(...)
meta_t *
next_chunk_header(meta_t *h)
{
    //Your code here
}

// helper function that returns a pointer to the previous (i.e. preceding) chunk's header
// when given pointer to this chunk's header. If this chunk is the first chunk on the heap,
// returns NULL
// To be used by coalesce_prev(..)
meta_t *
prev_chunk_header(meta_t *h)
{
    //Your code here
}

// Return a pointer to the first chunk of the implicit list
// If the list is empty, return NULL
// the implementation has been given to you below.
meta_t *
first_chunk_header()
{
    //the first chunk, if exists, starts at 8-byte offset from the base of the heap
    //in order for payload to be aligned at 16-byte boundary
    void *h = (void *)mem_heap_lo() + ALIGNMENT/2;
    if (h >= mem_heap_hi()) {
        return NULL;
    } else {
        return h;
    }
}
// helper function ask_os_for_chunk invokes the mem_sbrk function to ask for a chunk of 
// memory (of size csz) from the "operating system". It initializes the new chunk 
// using helper function init_chunk and returns the initialized chunk.
// To be used by mm_malloc when not enough space is found on existing heap
// the implementation has been given to you below.
meta_t *
ask_os_for_chunk(size_t csz)
{
    //must ask OS for allocation in multiples of ALIGNMENT
    assert(csz % ALIGNMENT == 0);

    void *p = (meta_t *)mem_sbrk(csz);
    //the returned pointer must be at ALIGNMENT/2 offset from alignment
    assert( (((unsigned long)p)%ALIGNMENT) == ALIGNMENT/2);
	init_chunk(p, csz, false);
	return p;
}

// helper function that initializes a chunk's header and footer field
// Used by ask_os_for_chunk(...)
void
init_chunk(meta_t *p, size_t csz, bool allocated)
{
    //Your code here
}

// mm_init is called to initialize the malloc library
// it sets up ALIGNMENT/2 bytes of padding so the first chunk's
// payload is aligned at a 16-byte boundary.
// the implementation has been given to you below.
int 
mm_init(void)
{
    assert(2*sizeof(meta_t) == ALIGNMENT);
    //put a 8-byte padding in the beginning of the heap
    //other than that, the heap starts out empty
    mem_sbrk(sizeof(meta_t)); 
	return 0;
}

//
// mm_checkheap checks the integrity of the heap and returns a struct containing 
// basic statistics about the heap. Use helper function first_chunk_header and 
// next_chunk_header for traversing the heap
heap_info_t
mm_checkheap(bool verbose)
{
	heap_info_t info = {0};
	// Your code here
	//
	// traverse the heap to fill in the fields of info

	// correctness of implicit heap amounts to the following assertion:
	// the sum of all chunk sizes equals the heap size minus the ALIGNMENT/2
	// padding bytes set up by mm_init.
	assert(mem_heapsize() - ALIGNMENT/2 == (info.allocated_size + info.free_size));
	return info;
}

// helper function to traverse the entire heap chunk by chunk from the begining. 
// It returns the first free chunk encountered whose size is bigger or equal to "csz".  
// It returns NULL if no large enough free chunk is found.
// Use helper function first_chunk_header and next_chunk_header for traversing the heap
// To be used by mm_malloc
meta_t *
first_fit(size_t csz)
{
	//Your code here
}

// helper function to split an allocated chunk into two chunks. 
// The first one has size "csz", and the second one contains the remaining bytes. 
// You must check that the size of the original chunk is big enough to enable the split.
// To be used by mm_malloc
void
split(meta_t *original, size_t csz)
{
	//Your code here
}



//mm_malloc allocates a memory block of size bytes
//and returns a pointer aligned to ALIGNMENT
void *
mm_malloc(size_t size)
{
	//make requested payload size aligned
	size = align(size);
	//chunk size is aligned because payload and header+footer size
	//are both aligned
	size_t csz = 2*hdr_size + size;
    
	//Your code here 
	//
	//Your code logic should be:
	//Try to find a free chunk using helper function first_fit
	//    If found, split the chunk (using helper function split).
	//    If not found, ask OS for new memory using helper ask_os_for_chunk
	//Set the chunk's status to be allocated


	//After finishing obtaining free chunk p, 
	//check heap correctness to catch bugs
	if (debug) {
		mm_checkheap(true);
	}
	return NULL;
}

// helper function to merge the current chunk with the next/following chunk
// if the next chunk is free, and returns a pointer to the merged chunk's header.
// You only need to consider merging with the immediate next chunk because 
// your heap would never contain two consecutive free chunks.
// To be used by mm_free(...)
meta_t *
coalesce_next(meta_t *h)
{
    //Your code here
}

// helper function to merge the current chunk with the prev/proceding chunk
// if the previous chunk is free, and returns a pointer to the 
// merged chunk's header (if merged) or the current chunk (if not merged.
// You only need to consider merging with the immediate previous chunk because 
// your heap would never contain two consecutive free chunks.
// To be used by mm_free(...)
meta_t *
coalesce_prev(meta_t *h)
{
    //Your code here
}


// mm_free frees the previously allocated memory block
void 
mm_free(void *p)
{
	// Your code here
	//
	// The code logic should be:
	// 1. Obtain pointer to current chunk's header using helper payload2header
	// 2. Set current chunk's status (header AND footer) to "free"
	// 3. Call coalesce_next() to merge with the next chunk if it is free
	// 4. Call coalesce_prev() to merge with the previous chunk if it is free
	  
	  
	// After freeing the chunk, check heap correctness to catch bugs
	if (debug) {
		mm_checkheap(true);
	}
}	

// mm_realloc changes the size of a previous allocation (pointed to by ptr) to size bytes,  
// and returns a pointer to the new allocation.
// If the new allocation is at a different address than the existing allocation. You need to 
// copy the contents over (using memmove from std string library <string.h> to do this).
// Note that since the new size can be smaller than the old size, the contents copied 
// will be the minimum of the old and new size.
// If ptr is NULL, then the call is equivalent to  malloc(size).
// if size is equal to zero, and ptr is not NULL, then the call is equivalent to free(ptr).
void *
mm_realloc(void *ptr, size_t size)
{
	// Your code here
	  
	// Check heap correctness after realloc to catch bugs
	if (debug) {
		mm_checkheap(true);
	}
	return NULL;
}



