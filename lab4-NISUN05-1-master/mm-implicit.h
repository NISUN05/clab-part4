#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
typedef uint64_t meta_t; // header or footer type (meta_t) is 8-byte unsigned int

extern size_t hdr_size; 

//helper functions of mm-implicit.c
size_t get_size(meta_t *h);
bool get_status(meta_t *h);
void set_size_status(meta_t *h, size_t csz, bool allocated);
void set_status(meta_t *h, bool allocated);

meta_t *payload2header(void *p);
meta_t *header2footer(meta_t *h);

meta_t *next_chunk_header(meta_t *h);
meta_t *prev_chunk_header(meta_t *h);
meta_t *first_chunk_header();
meta_t *first_fit(size_t csz);
void split(meta_t *original, size_t csz);
void init_chunk(meta_t *h, size_t csz, bool allocated);
meta_t *ask_os_for_chunk(size_t csz);
meta_t *coalesce_next(meta_t *h);
meta_t *coalesce_prev(meta_t *h);
