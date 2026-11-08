// Original: volition/rfg/code/util/special_allocators.h
// Implementation: volition/rfg/code/util/special_allocators.cpp
//
// Special allocators: memory borrowed from the level pools for short phases (salloc_*). The pregame split lock lends
// part of the split memory manager before gameplay starts; the gameplay CPU/GPU locks lend a mempool during gameplay,
// optionally asynchronously (the lock completes once streaming has released the memory).
#pragma once


class mempool_base;
class rfg_split_mempool;

void salloc_get_pregame_split_lock_max_sizes(int& cpu_size, int& gpu_size); // 0x00B61E00 (:45)
rfg_split_mempool* salloc_pregame_split_lock(int cpu_size, int gpu_size); // 0x00B62600 (:62)
void salloc_pregame_split_unlock(rfg_split_mempool* pool); // 0x00B62900 (:87)
void salloc_get_gameplay_gpu_lock_size(int& gpu_size); // 0x00B61E20 (:102)
mempool_base* salloc_gameplay_gpu_lock(); // 0x00B61E30 (:108)
bool salloc_gameplay_gpu_lock_async(); // 0x00B61E60 (:134)
mempool_base* salloc_gameplay_gpu_lock_async_poll(); // 0x00B61E70 (:139)
void salloc_gameplay_gpu_unlock(); // 0x00B62390 (:144)
void salloc_gameplay_gpu_unlock_async(); // 0x00B61E80 (:154)
bool salloc_gameplay_gpu_unlock_async_poll(); // 0x00B61E90 (:159)
mempool_base* salloc_gameplay_cpu_lock(); // 0x00B61EA0 (:164)
void salloc_gameplay_cpu_unlock(); // 0x00B61EB0 (:169)
void salloc_gameplay_cpu_unlock_async(); // 0x00B61EC0 (:174)
bool salloc_gameplay_cpu_unlock_async_poll(); // 0x00B61ED0 (:179)
void salloc_get_gameplay_cpu_lock_size(int& cpu_size); // 0x00B61EE0 (:184)
