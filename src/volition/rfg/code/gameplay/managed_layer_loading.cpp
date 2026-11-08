// Original: volition/rfg/code/gameplay/managed_layer_loading.cpp
//
// Managed layer sets (decompiled). A set names up to 16 layer files; each file name resolves (through
// rfg_zonex_get_territory_info) to a layer grid id (grid x, grid y, layer index), the upper 16 bits of the handles of the
// objects the layer creates. Loading requests the set's stream2 container when a slot of its size is free;
// serialize_layer_objects creates the layer objects through world::serialize_layer once the container is loaded and
// records their handles in All_manager_layer_objects; unloading releases the container and destroys the recorded
// objects of the set's layers (and the ones that no longer exist).
//
// Notes (decompiled behaviour, kept as is):
// - load passes 0x40 as the size of its 6144-byte file name buffer to cf_drop_ext.
// - level_init (0x0078DB00) is a 16-byte stub that runs start_layer_unloading.

#include "volition/rfg/code/gameplay/managed_layer_loading.h"
#include "volition/rfg/code/gameplay/managed_layer_loading_shared.h"

#include "volition/rfg/code/level/rfg_zonex_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/rfg.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include "volition/rfg/code/gamewide/game_trace.h"

#include "keen/base/trace.hpp"

#include <string.h>

// ---- globals
farray<unsigned int,512> All_manager_layer_objects; // 0x01657838
string_pool Managed_layer_name_string_pool("managed layer names"); // 0x02231C00, name inferred
object_pool<managed_layer_set,512,1> Managed_layer_set_pool; // 0x022522F0

// ---- file-static globals
static char* Managed_layer_name_string_pool_data; // 0x02229658, buffer of Managed_layer_name_string_pool

// Slots per size: 3 small sets or 1 large set hold container references at once.
static int slot_cap_count(bool large_slot)
{
    return large_slot ? 1 : 3;
}

// ---- internal functions

// 0x0078D670 (:43)
// Number of sets of the given slot size that currently hold container references.
static int get_slot_count(bool large_slots)
{
    int count = 0;
    csspinlock_scope lock(Managed_layer_set_pool.thread_access);
    for (managed_layer_set* set = Managed_layer_set_pool.get_first(0); set != nullptr; set = Managed_layer_set_pool.get_next(set))
    {
        if (set->large_slot_type() == large_slots && set->using_slot())
        {
            count++;
        }
    }
    return count;
}

// ---- managed_layer_info

// 0x007692C0 (.h:43)
managed_layer_info::managed_layer_info()
{
    file_name = nullptr;
}

// 0x0075E650 (:64)
void managed_layer_info::reset()
{
    file_name = nullptr;
}

// 0x0075E660 (:69)
bool managed_layer_info::member_of_layer(unsigned int handle) const
{
    return (unsigned int)layer_grid_id == handle >> 16;
}

// ---- managed_layer_set

// 0x0077EDC0 (.h:65)
managed_layer_set::managed_layer_set()
{
    reset();
}

// 0x007692D0 (.h:134)
void managed_layer_set::reset()
{
    managed_layers.remove_all();
    srid = 0;
    layer_flags.large_slot = false;
    layer_flags.layer_serialized = false;
}

// 0x0078D730 (:82)
bool managed_layer_set::load(const char* layer_set_name, xml_element* xml_root, bool large_slot_value)
{
    char territory_name[6144];
    char file_name[6144];
    int grid_x;
    int grid_y;
    int grid_z = 0;

    xml_element* info = xml_find(xml_root, "managed_layer_info");
    if (info == nullptr)
    {
        return false;
    }
    reset();
    for (int i = 0; i < 16; i++)
    {
        const xml_element* tag = xml_find(info, XML_MANAGED_LAYER_FILE_NAME_TAGS[i]);
        if (tag == nullptr)
        {
            continue;
        }
        // 0x40, not sizeof(file_name) (see the notes above).
        cf_drop_ext(file_name, 0x40, tag->text);
        const char* name = Managed_layer_name_string_pool.add_unique(file_name);
        if (name == nullptr)
        {
            return false;
        }
        if (!rfg_zonex_get_territory_info(name, territory_name, grid_x, grid_y, grid_z))
        {
            VLIB_CRITICAL_FAILURE();
            return false;
        }
        managed_layer_info layer;
        layer.file_name = name;
        // Handle layout: zone x in bits 12..15, zone y in bits 8..11, layer index in bits 0..7.
        layer.layer_grid_id = (int)(((((unsigned int)grid_x & 0xF) << 4) | ((unsigned int)grid_y & 0xF)) << 8 | ((unsigned int)grid_z & 0xFF));
        managed_layers.add(layer);
    }
    if (managed_layers.size() < 1)
    {
        return false;
    }
    layer_flags.large_slot = large_slot_value;
    srid = stream2_container_find(layer_set_name);
    GAME_TRACE("layer", "set %s: %d layer files, first %s grid 0x%04X, container %s", layer_set_name, managed_layers.size(), managed_layers[0].file_name, (unsigned int)managed_layers[0].layer_grid_id, srid != 0 ? "found" : "NOT FOUND");
    return srid != 0;
}

// 0x0079D550 (:140)
bool managed_layer_set::start_layer_loading()
{
    if (stream2_get_ref_count(srid) < 1 && get_slot_count(layer_flags.large_slot) >= get_slot_cap_count())
    {
        GAME_TRACE("layer", "load of %s refused: %d of %d slots used", managed_layers[0].file_name, get_slot_count(layer_flags.large_slot), get_slot_cap_count());
        return false;
    }
    if (!layer_loading_started() && !layer_resources_loaded())
    {
        GAME_TRACE("layer", "load requested: %s (%d files)", managed_layers[0].file_name, managed_layers.size());
        stream2_request(srid);
    }
    else
    {
        GAME_TRACE("layer", "load of %s: already %s", managed_layers[0].file_name, layer_resources_loaded() ? "loaded" : "loading");
    }
    return true;
}

// 0x0078D8D0 (:159)
void managed_layer_set::start_layer_unloading()
{
    if (!layer_loading_started())
    {
        return;
    }
    GAME_TRACE("layer", "unload: %s", managed_layers.size() > 0 ? managed_layers[0].file_name : "(none)");
    layer_flags.layer_serialized = false;
    stream2_release(srid);
    int i = 0;
    while (i < All_manager_layer_objects.size())
    {
        const unsigned int handle = All_manager_layer_objects[i];
        if (World.object_from_handle(handle) == nullptr || member_of_layer(handle))
        {
            World.destroy_object(handle, false);
            All_manager_layer_objects.remove_unsorted(i);
        }
        else
        {
            i++;
        }
    }
}

// 0x00774800 (:212)
bool managed_layer_set::layer_loading_started() const
{
    if (stream2_get_ref_count(srid) > 0)
    {
        return true;
    }
    switch (stream2_get_status(srid))
    {
    case ST_UNLOADED:
        return false;
    case ST_LOADING:
    case ST_ABORTING:
    case ST_LOADED:
        return true;
    case ST_ERROR:
        VLIB_CRITICAL_FAILURE();
        return true;
    default:
        VLIB_CRITICAL_FAILURE();
        return false;
    }
}

// 0x0075E680 (:267)
bool managed_layer_set::layer_resources_loaded() const
{
    switch (stream2_get_status(srid))
    {
    case ST_UNLOADED:
    case ST_LOADING:
        return false;
    case ST_LOADED:
        return true;
    default:
        VLIB_CRITICAL_FAILURE();
        return false;
    }
}

// 0x00774880 (:252)
bool managed_layer_set::layer_resources_serialized() const
{
    switch (stream2_get_status(srid))
    {
    case ST_UNLOADED:
    case ST_LOADING:
        return false;
    case ST_LOADED:
        return layer_flags.layer_serialized;
    default:
        VLIB_CRITICAL_FAILURE();
        return false;
    }
}

// 0x0078D990 (:302)
bool managed_layer_set::serialize_layer_objects()
{
    unsigned int handles[512];
    int num_handles;

    switch (stream2_get_status(srid))
    {
    case ST_UNLOADED:
    case ST_LOADING:
        return false;
    case ST_LOADED:
        break;
    default:
        VLIB_CRITICAL_FAILURE();
        return false;
    }
    if (layer_flags.layer_serialized)
    {
        return true;
    }
    for (int i = 0; i < managed_layers.size(); i++)
    {
        if (!World.serialize_layer(managed_layers[i].file_name, handles, 512, num_handles))
        {
            GAME_TRACE("layer", "serialize of %s failed", managed_layers[i].file_name);
            continue;
        }
        GAME_TRACE("layer", "serialized %s: %d objects", managed_layers[i].file_name, num_handles);
        for (int j = 0; j < num_handles; j++)
        {
            if (All_manager_layer_objects.full())
            {
                return false;
            }
            if (All_manager_layer_objects.add(handles[j]) < 0)
            {
                return false;
            }
        }
    }
    layer_flags.layer_serialized = true;
    return true;
}

// 0x00774910 (:395)
bool managed_layer_set::member_of_layer(unsigned int handle) const
{
    // Layer grid ids have a layer index of 0x80..0xFE; 0xFF is a zone's own handles.
    const unsigned int layer = (handle >> 16) & 0xFF;
    if (layer == 0xFF || layer < 0x80)
    {
        return false;
    }
    for (int i = 0; i < managed_layers.size(); i++)
    {
        if (managed_layers[i].member_of_layer(handle))
        {
            return true;
        }
    }
    return false;
}

// 0x0078DB00 (:344)
void managed_layer_set::level_init()
{
    start_layer_unloading();
}

// 0x0075E6E0 (:351)
bool managed_layer_set::using_slot() const
{
    return stream2_get_ref_count(srid) > 0;
}

// 0x0078DB10 (:358)
bool managed_layer_set::resource_slot_free() const
{
    if (stream2_get_ref_count(srid) > 0)
    {
        return true;
    }
    return get_slot_count(layer_flags.large_slot) < get_slot_cap_count();
}

// 0x007748E0 (:373)
bool managed_layer_set::is_dlc()
{
    const char* name = managed_layers[0].file_name;
    return name != nullptr && strstr(name, "dlc") != nullptr;
}

// 0x0075E700 (:384)
int managed_layer_set::get_slot_cap_count() const
{
    return slot_cap_count(layer_flags.large_slot);
}

// 0x00754DB0 (.h:110)
bool managed_layer_set::large_slot_type() const
{
    return layer_flags.large_slot;
}

// ---- system

// 0x007A57A0 (:432)
void managed_layer_system_init()
{
    if (Managed_layer_name_string_pool_data == nullptr)
    {
        // 0x1800 bytes of layer file names.
        Managed_layer_name_string_pool_data = (char*)Permanent_mempool.static_mempool_base::alloc(0x1800, 1);
        if (Managed_layer_name_string_pool_data == nullptr)
        {
            keen::debug::nativePrintArgs("Insufficient memory to allocate the managed layer string pool");
        }
        Managed_layer_name_string_pool.init(Managed_layer_name_string_pool_data, 0x1800, nullptr, 0, 0);
    }
    Managed_layer_name_string_pool.clear();
    csspinlock_scope lock(Managed_layer_set_pool.thread_access);
    managed_layer_set* set = Managed_layer_set_pool.get_first(0);
    while (set != nullptr)
    {
        set = Managed_layer_set_pool.free(set);
    }
}

// 0x00774960 (:446)
void managed_layer_level_init()
{
    All_manager_layer_objects.remove_all();
}

// 0x0078DB60 (:460)
void managed_layer_level_shutdown()
{
    csspinlock_scope lock(Managed_layer_set_pool.thread_access);
    for (managed_layer_set* set = Managed_layer_set_pool.get_first(0); set != nullptr; set = Managed_layer_set_pool.get_next(set))
    {
        set->start_layer_unloading();
    }
}

// 0x0079D5D0 (:473)
managed_layer_set* managed_layer_load(const char* layer_set_name, xml_element* xml_root, bool large_slot)
{
    if (xml_root == nullptr || layer_set_name == nullptr)
    {
        return nullptr;
    }
    managed_layer_set* set = Managed_layer_set_pool.alloc(0);
    if (set == nullptr)
    {
        return nullptr;
    }
    if (set->load(layer_set_name, xml_root, large_slot))
    {
        return set;
    }
    Managed_layer_set_pool.free(set);
    return nullptr;
}

// 0x0075E720 (:493)
const farray<unsigned int,512>& manager_layer_all_objects()
{
    return All_manager_layer_objects;
}
