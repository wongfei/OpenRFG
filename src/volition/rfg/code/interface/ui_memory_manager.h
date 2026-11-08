// Original: volition/rfg/code/interface/ui_memory_manager.h
// Implementation: volition/rfg/code/interface/ui_memory_manager.cpp
//
// The UI memory manager: the GSP_VDOC stream2 callbacks, the doc and peg srids of each document, the interface
// allocators (permanent, frontend, in-game, multiplayer) and the game's bitmap sheets. Placement of game_sheet_info
// inferred.
#pragma once

#include "volition/rfg/code/interface/ui_document_shared.h"
#include "volition/stream2/code/stream2_allocator.h"
#include "volition/vlib/code/util/varray.h"

// size 0x54
struct umm_allocation_data
{
    umm_allocation_data(); // 0x008A0020 (:46)

    char name[64]; // 0x00
    doc_resource_type type; // 0x40
    unsigned char* cpu_data; // 0x44
    unsigned char* gpu_data; // 0x48
    unsigned int cpu_size; // 0x4C
    unsigned int gpu_size; // 0x50
};

// size 0x20
struct game_sheet_info
{
    char m_name[32]; // 0x00
};

extern farray<vint_doc_stream_info*,16> Stream_requests; // 0x01667648
extern stream2_mempool_allocator Stream_interface_allocator; // 0x01667614
extern stream2_system_allocator Stream_interface_stack_allocator; // 0x01667620
extern unsigned int Num_sheets; // 0x02C0819C
extern stream2_slot_allocator Stream_interface_slot_allocator; // 0x01667624

vint_doc_stream_info* mem_manager_find_doc_stream_info(unsigned int srid); // 0x008A0160 (:150)
vint_doc_stream_info* mem_manager_find_doc_stream_info(const char* name); // 0x008A01A0 (:162)
critical_resource_data* mem_manager_find_meta_data_entry(const char* name, unsigned int srid); // 0x008A01F0 (:177)
critical_resource_data* mem_manager_find_crit_resource(const char* name, unsigned int srid); // 0x008A0290 (:197)
void game_sheet_info_reload(game_sheet_info* gsi); // 0x008A0430 (:274)
bool mem_manager_initialize(); // 0x008A04E0 (:296)
bool mem_manager_initialize_allocators(); // 0x008A0510 (:307)
void mem_manager_shutdown(); // 0x008A0580 (:326)
void mem_manager_process(); // 0x008B72B0 (:341)
void mem_manager_force_synchronous_process(); // 0x008B7380 (:390)
bool mem_manager_add_preloaded_peg(const char* name); // 0x008A05C0 (:429)
bool mem_manager_add_sheet(const char* name); // 0x008B74A0 (:443)
int mem_manager_has_sheet(const char* name); // 0x008A0660 (:466)
unsigned int mem_manager_get_num_sheets(); // 0x008A06B0 (:510)
const char* mem_manager_get_sheet(int which); // 0x008A06C0 (:530)
void mem_manager_request_memory(unsigned int cpu_size, unsigned int gpu_size, const char* name, unsigned int cpu_alignment, unsigned int gpu_alignment, void*& cpu_mem, void*& gpu_mem); // 0x008CABA0 (:563)
void* mem_manager_request_memory(unsigned int size, const char* name, unsigned int alignment); // 0x008CACB0 (:592)
bool mem_manager_free_requested_memory(const char* name); // 0x008B74F0 (:601)
void mem_manager_get_cpu_gpu_peg_alignments(int& cpu_align, int& gpu_align); // 0x008A06E0 (:616)
bool mem_manager_doc_in_loading_q(vint_doc_stream_info* stream_info); // 0x008B75A0 (:624)
unsigned int mem_manager_get_doc_srid(const char* name); // 0x008A0720 (:630)
bool mem_manager_load_doc(unsigned int srid); // 0x008CACE0 (:640)
void mem_manager_unload_doc(unsigned int srid); // 0x008CADA0 (:670)
void interface_pegs_loaded_callback(unsigned int srid); // 0x008EFA00 (:712)
void interface_pegs_unloaded_callback(unsigned int srid); // 0x008A0740 (:729)
void interface_doc_loaded_callback(unsigned int srid); // 0x008EFA80 (:735)
void interface_doc_unloaded_callback(unsigned int srid); // 0x008A0750 (:753)
bool interface_vint_doc_loaded_callback(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size); // 0x008A0760 (:759)
void interface_vint_doc_unloaded_callback(const char* filename, unsigned int srid); // 0x008A07C0 (:775)
bool interface_script_loaded_callback(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size); // 0x008A0820 (:790)
void interface_script_unloaded_callback(const char* filename, unsigned int srid); // 0x008A0860 (:805)
void ui_set_thread_ownership(); // 0x008A0890 (:817)
