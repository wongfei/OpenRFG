// Original: volition/rfg/code/gamewide/asset_assembler.h
// Implementation: volition/rfg/code/gamewide/asset_assembler.cpp
//
// The asset assembler: assasm_start opens a system, assasm_container_start/_add/_finish describe each container and
// its primitives (with their peg and slw files), and assasm_finish ends it, optionally handing back the assembled peg.
// With load_from_preload the containers come from the system's .asm_pc (the packfile game); otherwise they are built
// through stream2 (no packfiles, and systems always assembled at run time such as the foliage containers).
#pragma once


class mempool_base;
struct pasm_data;
class stream2_prim;

bool assasm_is_preload_file(const char* filename); // 0x007D7DE0 (:56)
bool assasm_start(const char* system_name, mempool_base* tmp_pool, mempool_base* container_pool, bool load_from_preload, mempool_base* peg_pool, bool build_peg, bool is_temp, bool validate, bool patch_data); // 0x007B9AE0 (:141)
bool assasm_container_start(const char* widget_name, unsigned char container_type, bool preload, bool passive); // 0x007B9C20 (:200)
stream2_prim* assasm_container_add(const char* item_name, unsigned char prim_type, const char* category, bool add_peg, bool add_slw, bool report_errors, const char* peg_category, unsigned char allocator); // 0x007E8890 (:218)
bool assasm_add_slw3(const char* slw_file, const char* category, bool report_errors); // 0x007D7E20 (:277)
unsigned int assasm_container_finish(bool no_preload, bool is_temp, bool patch); // 0x007CAFD0 (:330)
bool assasm_finish(pasm_data* peg_data, bool verify_only, bool pack_pointers, bool force_no_data_generate); // 0x007B9CB0 (:365)
