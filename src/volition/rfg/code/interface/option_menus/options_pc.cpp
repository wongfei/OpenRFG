// Original: volition/rfg/code/interface/option_menus/options_pc.cpp
//
// The option menus' default hints on PC (UI.md UI6): the three hint groups of an options document, their text and
// button icon objects, and the mouse on them (the icons hidden, the groups clickable).

#include "volition/rfg/code/interface/option_menus/option_menu_system.h"

#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/vlib/code/math/crc.h"

// ---- file-static globals
static const char* Clickable_names[3] = {"hint_1_group", "hint_1_group2", "hint_1_group3"}; // 0x016630C8
static const char* Options_hint_text_element = {"hints_text_1"}; // 0x016630D4
static const char* Options_hint_icon_element = {"hint_button_1"}; // 0x016630D8
// static unsigned int $S6; // 0x02CA1534 (a static local)

// ---- external interface

// 0x00888560 (.h:95)
options_default_hint_objects::options_default_hint_objects()
{
    for (int i = 0; i < 3; i++)
    {
        group_objects[i] = nullptr;
        icon_objects[i] = nullptr;
        text_objects[i] = nullptr;
    }
}

// 0x008D6FB0 (:9)
// LTCG: doc_handle in EDI. Each hint group and, in it, its text and icon.
void options_menu_get_default_hint_elements(int doc_handle, options_default_hint_objects& objects)
{
    // Static locals (guarded by $S6 in rfg.exe).
    static const unsigned int text_obj_crc = crc_stri(Options_hint_text_element, 0);
    static const unsigned int icon_obj_crc = crc_stri(Options_hint_icon_element, 0);
    vint_doc_info* info = ui_get_doc_info(doc_handle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    for (int i = 0; i < 3; i++)
    {
        objects.group_objects[i] = vint_object_base::find_object(Clickable_names[i], info->m_doc);
        if (objects.group_objects[i] != nullptr)
        {
            objects.text_objects[i] = objects.group_objects[i]->find_child(text_obj_crc);
            objects.icon_objects[i] = objects.group_objects[i]->find_child(icon_obj_crc);
        }
    }
}

// 0x008C4880 (:26)
// The icons hidden; the clicked group's index (or -1).
int options_menu_process_mouse_on_default_hints(options_default_hint_objects& objects)
{
    for (int i = 0; i < 3; i++)
    {
        if (objects.icon_objects[i] != nullptr)
        {
            objects.icon_objects[i]->set_property_value("visible", vint_variant(false), 0);
        }
    }
    return ui_mouse_process_clickables(objects.group_objects, 3, true);
}
