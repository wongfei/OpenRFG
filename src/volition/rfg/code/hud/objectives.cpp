// Original: volition/rfg/code/hud/objectives.cpp
//
// The objective table (objectives.xtbl, mission_objectives.xtbl and the DLC's), which objectives are active and their
// map icons. fullscreen_map_init loads the table; init_objectives has no callers in rfg.exe.

#include "volition/rfg/code/hud/objectives.h"

#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/hud/minimap_icon_info.h"
#include "volition/rfg/code/interface/ui_document_hud_missions.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/util/fastxml.h"

#include <stdio.h>
#include <string.h>

// ---- file-static globals
static farray<objective,256> objectives; // 0x02B9DDD0
static unsigned int Dlc_objectives_mark; // 0x02B957F4

void create_objective(const char* name, const char* description, const char* display_name, bool mission);

// 0x008829D0 (:18)
void load_objectives()
{
    objectives.clear();
    if (Multiplayer)
    {
        return;
    }
    const unsigned int mark = Common_mempool.mark();
    xml_element* root = xml_parse("objectives.xtbl", &Common_mempool);
    if (root == nullptr)
    {
        return;
    }
    xml_element* table = xml_find(root, "Table");
    if (table == nullptr)
    {
        return;
    }
    for (xml_element* entry = xml_find(table, "Objective"); entry != nullptr; entry = xml_find_next(table, entry, "Objective"))
    {
        xml_element* name = xml_find(entry, "Name");
        if (name == nullptr)
        {
            return;
        }
        xml_element* description = xml_find(entry, "Description");
        if (description == nullptr)
        {
            return;
        }
        create_objective(name->text, description->text, name->text, false);
    }
    root = xml_parse("mission_objectives.xtbl", &Common_mempool);
    if (root == nullptr)
    {
        return;
    }
    table = xml_find(root, "Table");
    if (table == nullptr)
    {
        return;
    }
    for (xml_element* entry = xml_find(table, "Mission_Objective"); entry != nullptr; entry = xml_find_next(table, entry, "Mission_Objective"))
    {
        xml_element* name = xml_find(entry, "Name");
        if (name == nullptr)
        {
            return;
        }
        xml_element* description = xml_find(entry, "Description");
        if (description == nullptr)
        {
            return;
        }
        xml_element* display_name = xml_find(entry, "Display_Name");
        if (display_name == nullptr)
        {
            return;
        }
        create_objective(name->text, description->text, display_name->text, true);
    }
    // Original bug, kept: every early return above leaves the pool's mark unrestored.
    Common_mempool.restore_to_mark(mark);
}

// 0x00882B60 (:71)
void init_dlc_objectives(int dlc_id)
{
    const unsigned int mark = Common_mempool.mark();
    Dlc_objectives_mark = (unsigned int)objectives.size();
    char filename[256];
    sprintf(filename, "dlc%02d_mission_objectives.xtbl", dlc_id);
    xml_element* root = xml_parse(filename, &Common_mempool);
    if (root == nullptr)
    {
        return;
    }
    xml_element* table = xml_find(root, "Table");
    if (table == nullptr)
    {
        return;
    }
    for (xml_element* entry = xml_find(table, "Mission_Objective"); entry != nullptr; entry = xml_find_next(table, entry, "Mission_Objective"))
    {
        xml_element* name = xml_find(entry, "Name");
        if (name == nullptr)
        {
            return;
        }
        xml_element* description = xml_find(entry, "Description");
        if (description == nullptr)
        {
            return;
        }
        xml_element* display_name = xml_find(entry, "Display_Name");
        if (display_name == nullptr)
        {
            return;
        }
        create_objective(name->text, description->text, display_name->text, true);
    }
    // Original bug, kept: the early returns above leave the pool's mark unrestored.
    Common_mempool.restore_to_mark(mark);
}

// 0x00881870 (:107)
void remove_dlc_objectives()
{
    for (int i = objectives.size() - 1; (unsigned int)(i + 1) > Dlc_objectives_mark; i--)
    {
        objectives.remove(i);
    }
}

// 0x00882C80 (:116)
void init_objectives()
{
    load_objectives();
}

// 0x00864F80 (:122)
void objectives_level_init()
{
    for (int i = 0; i < objectives.size(); i++)
    {
        objectives[i].reset();
    }
}

// 0x008818E0 (:132)
void create_objective(const char* name, const char* description, const char* display_name, bool mission)
{
    if (objectives.size() >= objectives.capacity())
    {
        return;
    }
    if (objectives.contains(objective(name, "", "", false)) != 0)
    {
        return;
    }
    // Magic numbers: the sizes of objective's three strings.
    if (strlen(name) >= 32 || strlen(description) >= 256 || strlen(display_name) >= 32)
    {
        return;
    }
    // Original bug, kept: a description of 128 to 255 characters passes the test above and overruns this buffer.
    char clean_description[128];
    int length = 0;
    const int description_length = (int)strlen(description);
    for (int i = 0; i < description_length; i++)
    {
        const char c = description[i];
        if (c != '[' && c != ']')
        {
            clean_description[length++] = c;
        }
    }
    clean_description[length] = '\0';
    objectives.add(objective(name, clean_description, display_name, mission));
}

// 0x00864FC0 (:156)
void add_objective(int handle)
{
    if (handle < 0)
    {
        return;
    }
    objectives[handle].active = true;
    hud_ui_missions_add_objective(objectives[handle].display_name);
}

// 0x00875D00 (:169)
void remove_objective(int handle)
{
    if (handle < 0)
    {
        return;
    }
    objective& obj = objectives[handle];
    for (int i = obj.icons.size() - 1; i >= 0; i--)
    {
        Cartographer.remove_icon(obj.icons[i]);
    }
    obj.icons.clear();
    obj.active = false;
    hud_ui_missions_remove_objective();
}

// 0x00875D50 (:189)
void complete_objective(int handle, bool show_hud)
{
    if (handle < 0)
    {
        return;
    }
    objectives[handle].complete = true;
    remove_objective(handle);
    if (show_hud)
    {
        hud_ui_missions_complete_objective();
    }
}

// 0x00879D00 (:209)
void add_objective_icon(int handle, unsigned int object_handle, minimap_elements icon_bitmap, bool only_render_closest)
{
    if (handle < 0)
    {
        return;
    }
    if (Cartographer.does_icon_exist(object_handle) || Cartographer.does_sticky_icon_exist(object_handle))
    {
        remove_objective_icon(handle, object_handle);
    }
    // rfg.exe inlines the query, with an unsigned range test.
    if ((unsigned int)icon_bitmap > (unsigned int)MAP_ELEMENT_CAMERA_CONE || !minimap_element_is_objective_icon(icon_bitmap))
    {
        icon_bitmap = MAP_ELEMENT_MISSION_OBJECTIVE;
    }
    // rfg.exe inlines this call: a sticky icon that is not the player's marker.
    Cartographer.add_icon(object_handle, icon_bitmap, true, only_render_closest, false);
    objective& obj = objectives[handle];
    if (obj.icons.contains(object_handle) == 0)
    {
        obj.icons.add(object_handle);
    }
}

// 0x00875D90 (:240)
void remove_objective_icon(int handle, unsigned int object_handle)
{
    if (handle < 0)
    {
        return;
    }
    objective& obj = objectives[handle];
    if (obj.icons.contains(object_handle) != 0)
    {
        obj.icons.remove_matching(object_handle);
    }
    Cartographer.remove_icon(object_handle);
}

// 0x00875DF0 (:259)
int objective_get_index_from_name(const char* name)
{
    return objectives.find(objective(name, "", "", false), false);
}

// 0x00864FF0 (:268)
int get_num_active_objectives()
{
    int count = 0;
    for (int i = 0; i < objectives.size(); i++)
    {
        const objective& obj = objectives[i];
        if (obj.active && mission_is_active() == obj.mission)
        {
            count++;
        }
    }
    return count;
}

// 0x00865030 (:289)
bool get_objective_position(vector& pos, const objective* obj)
{
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    bool found = false;
    for (int i = 0; i < objectives.size() && !found; i++)
    {
        if (&objectives[i] != obj)
        {
            continue;
        }
        for (int j = 0; j < objectives[i].icons.size(); j++)
        {
            const object* objp = World.object_from_handle(objectives[i].icons[j]);
            if (objp != nullptr)
            {
                pos = objp->pos;
                found = true;
                break;
            }
        }
    }
    return found;
}

// 0x008650F0 (:318)
bool is_objective_handle(unsigned int object_handle)
{
    for (int i = 0; i < objectives.size(); i++)
    {
        const objective& obj = objectives[i];
        for (int j = 0; j < obj.icons.size(); j++)
        {
            if (obj.icons[j] == object_handle)
            {
                return true;
            }
        }
    }
    return false;
}

// 0x00865140 (:339)
int get_total_num_objectives()
{
    return objectives.size();
}

// 0x00865150 (:345)
const char* get_objective_display_name(unsigned int which)
{
    return which < (unsigned int)objectives.size() ? objectives[(int)which].display_name : nullptr;
}

// 0x00865180 (:354)
const char* get_objective_description(unsigned int which)
{
    return which < (unsigned int)objectives.size() ? objectives[(int)which].description : nullptr;
}

// 0x008651A0 (:363)
bool get_objective_active(unsigned int which)
{
    return which < (unsigned int)objectives.size() ? objectives[(int)which].active : false;
}

// 0x008651C0 (:372)
bool get_objective_is_mission(unsigned int which)
{
    return which < (unsigned int)objectives.size() ? objectives[(int)which].mission : false;
}

// 0x008651E0 (:381)
bool get_objective_complete(unsigned int which)
{
    return which < (unsigned int)objectives.size() ? objectives[(int)which].complete : false;
}
