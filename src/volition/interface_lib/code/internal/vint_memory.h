// Original: volition/interface_lib/code/internal/vint_memory.h
// Implementation: volition/interface_lib/code/internal/vint_memory.cpp
//
// The library's memory: the permanent, temporary and XML pools given at init, the usage counter, and vint_realloc/
// vint_free through the game's memory callbacks.
#pragma once


class mempool;
class mempool_base;

extern mempool_base* Vint_perm_mempool; // 0x01A48624
extern mempool_base* Vint_temp_mempool; // 0x01A48628
extern mempool* Vint_xml_mempool; // 0x01A4862C
extern unsigned int Vint_memory_usage; // 0x01A48630

void* vint_realloc(void* _addy, unsigned int _bytes); // 0x0045BA70 (.h:54)
void vint_free(void* _addy); // 0x0045BA90 (.h:70)
