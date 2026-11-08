// Original: volition/vfxlib/code/vfx_effect_manager.h
// Implementation: volition/vfxlib/code/vfx_effect_manager.cpp
//
// The effect manager: pools of effects and effect templates (max_effects, max_effect_templates), template loading from
// memory or file and release.
#pragma once

#include "volition/render_lib/code/pool_list.h"

class mempool;
class vfx_effect;
class vfx_effect_template;

// size 0x48
class vfx_effect_manager
{
public:
    // size 0x18
    class effect_list : public pool_list<vfx_effect*>
    {
    public:
        effect_list(); // 0x00CAEB80
        virtual ~effect_list(); // vtbl+0x00 0x00CAEBA0

    };

    // size 0x18
    class effect_template_list : public pool_list<vfx_effect_template*>
    {
    public:
        effect_template_list(); // 0x00CAEBD0
        virtual ~effect_template_list(); // vtbl+0x00 0x00CAEBF0

    };

protected:
    vfx_effect_manager(); // 0x00CB4E00 (:17)
    ~vfx_effect_manager(); // 0x00CAECA0 (:22)
public:
    static vfx_effect_manager* create(); // 0x00CB4E50 (:27)
    void release(); // 0x00C9F030 (:37)
    void init(); // 0x00CAED10 (:42)
    vfx_effect_template* load_template(const unsigned char* data_p, unsigned char* gpu_data, const char* peg_filename, int& approx_gpu_size); // 0x00CBA430 (:104)
    vfx_effect_template* load_template(const char* filename, const char* extension, int byte_alignment, int& approx_gpu_size); // 0x00CBA4C0 (:120)
    void release_template(vfx_effect_template* template_p); // 0x00CA44F0 (:92)
    vfx_effect* get_new_effect(); // 0x00CAEE50 (:68)
    int register_effect(vfx_effect* effect_p); // 0x00CAEE80 (:76)
    void unregister_effect(vfx_effect* effect_p); // 0x00CAEED0 (:83)
    vfx_effect* get_effect(int);
    vfx_effect_template* get_template_by_name(char*);
    int get_template_count();
    vfx_effect_template* get_template(int);
    static void set_max_effects(int);
    static void set_max_effect_templates(int);
    static int get_max_effects();
    static int get_max_effect_templates();

    static const int max_effects_default; // 0x012C709C
    static const int max_effect_templates_default; // 0x012C70A0
private:
    static int max_effects; // 0x016F0A04
    static int max_effect_templates; // 0x016F0A08
    effect_list effects_freelist; // 0x00
    effect_list effects_registered; // 0x18
    effect_template_list effect_templates; // 0x30
};

vfx_effect_template* vfx_template_load_from_file(mempool* pool, const char* filename, const char* filename_extension, int byte_alignment, int& approx_gpu_size); // 0x00CBA630 (:183)
vfx_effect_template* vfx_template_load_from_memory(const unsigned char* data_p, unsigned char* gpu_data, int& approx_gpu_size); // 0x00CBA650 (:191)
void vfx_template_release(vfx_effect_template* effect_template_p); // 0x00CA4550 (:198)
