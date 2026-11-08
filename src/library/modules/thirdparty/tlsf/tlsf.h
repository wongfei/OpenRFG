// Original: library/modules/thirdparty/tlsf/tlsf.h (third party: Two-Level Segregated Fit allocator by Matthew Conte,
// version 2.x; recovered from rfg.exe)
//
// tlsf_create turns a memory block into a pool; the pool header (tlsf_overhead() bytes) lives at the start of the
// block. 32-bit build: 4-byte alignment, block sizes up to 1 GB.
#pragma once

#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef void* tlsf_pool;

tlsf_pool tlsf_create(void* mem, size_t bytes); // 0x004019D0 (tlsf.c:748)
void tlsf_destroy(tlsf_pool pool); // 0x00401A50 (tlsf.c:818)

void* tlsf_malloc(tlsf_pool pool, size_t bytes); // 0x00401A60 (tlsf.c:824)
void* tlsf_memalign(tlsf_pool pool, size_t align, size_t bytes); // 0x00401AC0 (tlsf.c:832)
void* tlsf_realloc(tlsf_pool pool, void* ptr, size_t size); // 0x00401C50 (tlsf.c:913)
void tlsf_free(tlsf_pool pool, void* ptr); // 0x00401BB0 (tlsf.c:886)

size_t tlsf_block_size(void* ptr); // 0x004019A0 (tlsf.c:722)
size_t tlsf_overhead(void); // 0x004019C0 (tlsf.c:738)

typedef void (*tlsf_walker)(void* ptr, size_t size, int used, void* user);
void tlsf_walk_heap(tlsf_pool pool, tlsf_walker walker, void* user); // 0x00401950 (tlsf.c:705)
int tlsf_check_heap(tlsf_pool pool); // 0x00401D80 (tlsf.c:640)

#if defined(__cplusplus)
}
#endif
