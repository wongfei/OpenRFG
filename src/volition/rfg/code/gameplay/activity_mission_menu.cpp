// Original: volition/rfg/code/gameplay/activity_mission_menu.cpp
//
// Partial: the activity and mission start menus (debug menus listing the mission and activity start nodes, launched
// by the "mission_start" / "activity_start" console commands). rfg.exe registers neither command, so only
// activity_mission_menu_init_post (called by activity_init_post) runs. Not ported, dead in rfg.exe:
// unused_dcf_mission_start (0x007AF700), unused_dcf_activity_start (0x007AF800), activity_node_get (0x0078AF60),
// mission_menu_launch (0x007A9F60), activity_menu_launch (0x007AA250), activity_mission_menu_do_frame (0x0075C140,
// empty, no caller), mission_less_than_or_equal_to (0x0075C0C0), the activity_submenu_setup<T> instances
// (0x00793C70 to 0x00794AE0) and mission_warp_and_start (0x007AF640).

#include "volition/rfg/code/gameplay/activity_mission_menu.h"

#include "volition/rfg/code/os/dc_menu.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/string_pool.h"

// ---- file-static globals
// Magic numbers: 9 menus (the activity menu and its 8 sub menus) and 160 items, from the pool instances.
static object_pool<dc_menu, 9, 1> Activity_mission_menu_pool; // 0x02231C70
static object_pool<dc_menu_item, 160, 1> Activity_mission_menu_item_pool; // 0x0226D390
static char Activity_mission_menu_string_pool_data[0x1A00]; // 0x02227A70
static string_pool Activity_mission_menu_string_pool(nullptr); // 0x02272D78

// ---- functions

// 0x007A9EA0 (:472)
// Empties both menu pools and points the string pool at its buffer.
void activity_mission_menu_init_post()
{
    Activity_mission_menu_pool.thread_access.enter();
    for (dc_menu* menu = Activity_mission_menu_pool.get_first(0); menu != nullptr; menu = Activity_mission_menu_pool.free(menu))
    {
    }
    Activity_mission_menu_pool.thread_access.leave();
    Activity_mission_menu_item_pool.thread_access.enter();
    for (dc_menu_item* item = Activity_mission_menu_item_pool.get_first(0); item != nullptr; item = Activity_mission_menu_item_pool.free(item))
    {
    }
    Activity_mission_menu_item_pool.thread_access.leave();
    Activity_mission_menu_string_pool.init(Activity_mission_menu_string_pool_data, sizeof(Activity_mission_menu_string_pool_data), "act_mis_menu", 0, 0);
    Activity_mission_menu_string_pool.clear();
}
