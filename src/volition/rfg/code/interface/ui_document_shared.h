// Original: volition/rfg/code/interface/ui_document_shared.h
// Implementation: volition/rfg/code/interface/ui_document_shared.cpp
//
// What the document manager and the memory manager share: a document's stream info (its doc and peg containers, srids,
// state; Vint_doc_stream_data holds at most 64) and its critical resources. Placement of doc_resource_type inferred.
#pragma once

#include "volition/vlib/code/vlib_core/vlib_core.h"

class mempool;
class mempool_base;

enum doc_resource_type
{
    DOC_RESOURCE_TYPE_INVALID = -1,
    DOC_RESOURCE_TYPE_PEG = 0,
    DOC_RESOURCE_TYPE_LUA_SCRIPT = 1,
    DOC_RESOURCE_TYPE_VINT_DOC = 2,
    NUM_DOC_RESOURCE_TYPES = 3,
};

// size 0x18
struct critical_resource_data
{
    critical_resource_data(); // 0x00888310 (.h:62)

    const char* name; // 0x00
    doc_resource_type type; // 0x04
    unsigned char* cpu_data; // 0x08
    unsigned char* gpu_data; // 0x0C
    unsigned int cpu_size; // 0x10
    unsigned int gpu_size; // 0x14
};

// size 0xBC
struct vint_doc_stream_info
{
    vint_doc_stream_info(); // 0x00888330 (.h:83)

    const char* name; // 0x00
    critical_resource_data vint_doc; // 0x04
    critical_resource_data meta_data_entries[2]; // 0x1C
    critical_resource_data critical_resources[4]; // 0x4C
    unsigned int num_meta_data_entries; // 0xAC
    unsigned int num_critical_resources; // 0xB0
    unsigned int srid; // 0xB4
    unsigned int srid2; // 0xB8
};

extern vint_doc_stream_info Vint_doc_stream_data[64]; // 0x02C40C60
// Placement inferred (public, no header in rfg.pdb): the documents in Vint_doc_stream_data.
extern unsigned int Num_vint_doc_stream_data; // 0x02C08194

bool ui_document_parse_doc_file(const char* name, const char* filename, mempool_base* temp_pool, vint_doc_stream_info& stream_data, vlib_platform plat); // 0x008B67E0 (:131)
void ui_document_create_doc_containers(mempool_base* tmp_pool, mempool* pool, bool load_from_preload, vlib_platform plat, bool patch, int dlc_id); // 0x008B6C50 (:332)
int findStreamedFiles(char** pResultBuffer, const char** pFiles, unsigned int fileCount, const char* pPreFix); // 0x0089E330 (:564)
void ui_document_create_stream_image_containers(mempool_base* tmp_pool, mempool* pool, bool load_from_preload, vlib_platform plat, int dlc_id); // 0x0089E340 (:627)
void ui_document_load_shared(mempool_base* tmp_pool, mempool* pool, bool load_from_preload, vlib_platform plat, bool patch, int dlc_id); // 0x008B7020 (:805)
