// Original: volition/interface_lib/code/objects/noninteractive/vint_element_video.cpp
//
// The video element (UI.md UI2): draws the game's video (by id) through the draw_video callback (a pool of two).
// Compiled in interface_lib_unity.cpp.

#include "volition/interface_lib/code/objects/noninteractive/vint_element_video.h"

#include "volition/interface_lib/code/internal/vint_prop_macros.h"

// ---- class statics
unsigned int vint_element_video::Int_obj_type = 0xFFFFFFFF; // 0x014F593C
const unsigned int& vint_element_video::Object_type = vint_element_video::Int_obj_type; // 0x014F5940

// ---- file-static globals
static vint_prop_lookup Prop_lookup_vint_element_video[1] = { // 0x014F6DD0
    VINT_PROP_ENTRY(vint_element_video, vid_id_handle, VINT_PROP_TYPE_UINT, 0, true),
};
static vint_prop_mgr Prop_mgr_vint_element_video; // 0x01A4F49C
static vint_object_factory_imp<vint_element_video> Factory_vint_element_video;

// 0x00468FF0 (.h:33)
vint_element_video::vint_element_video()
{
}

// 0x0045F0E0 (.h:34)
vint_element_video::~vint_element_video()
{
}

// 0x00469020 (.h:41)
bool vint_element_video::is_type(unsigned int _type) const
{
    return _type == Object_type || _type == vint_element_base::Object_type;
}

// 0x00469050 (.h:41)
unsigned int vint_element_video::get_type() const
{
    return Object_type;
}

// 0x004724E0 (:65)
bool vint_element_video::system_init()
{
    Int_obj_type = vint_register_object_class("video", vint_element_base::Object_type, Prop_mgr_vint_element_video, Prop_lookup_vint_element_video, 1, Factory_vint_element_video, 2);
    return true;
}

// 0x0045F0F0 (:72)
void vint_element_video::system_shutdown()
{
}

// 0x004756F0 (:77)
bool vint_element_video::init()
{
    if (!vint_element_base::init())
    {
        return false;
    }
    m_vid_id_handle = 0;
    return true;
}

// 0x0045F100 (:88)
void vint_element_video::clean_up()
{
}

// 0x00472580 (:92)
void vint_element_video::process()
{
}

// 0x00469060 (:97)
void vint_element_video::render_object(const vint_render_params& _render_params)
{
    if (Render_callbacks != nullptr && Render_callbacks->draw_video != nullptr)
    {
        Render_callbacks->draw_video(m_vid_id_handle);
    }
}

// 0x0045F110 (:123)
void vint_element_video::render_post_process()
{
}

// 0x0045F120 (:126), 0x0045F140 (:134)
VINT_PROP_WRAPPERS(vint_element_video, vid_id_handle)

// 0x00469080 (:127)
bool vint_element_video::get_vid_id_handle(vint_variant& _val, int _index) const
{
    _val = m_vid_id_handle;
    return true;
}

// 0x0045F160 (:135)
bool vint_element_video::set_vid_id_handle(const vint_variant& _val, int _index)
{
    m_vid_id_handle = (int)_val;
    return true;
}
