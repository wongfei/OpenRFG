// Original: volition/rfg/code/destruction/rfg_materials.cpp
//
// Destruction materials (see rfg_materials.h), read from rfg_effect_materials.xtbl, rfg_materials.xtbl,
// rfg_melee_groups.xtbl and rfg_bullet_groups.xtbl into Effect_material_pool.
//
// Partial: rfg_material_get_shape_type(const object*, unsigned int) (0x00725810, :1710, needs the object and mover
// classes) is not implemented yet.
//
// Notes (decompiled behaviour, kept as is):
// - Explode events name explosions; their handle is the explosion_info pointer. rfg_materials_system_init runs before
//   explosion_system_init in the game, so those names are not found then.
// - Every other event names effects. Which kind an event child names is decided by the loop variable of the event type
//   search, which the default-shape fill of a <default> block reuses (leaving it at 5): explode events with a
//   <default> block look their children up as effects.
// - An effect material reads up to 11 gust effects into an array of 10 (the 11th overlaps the driving fields, which
//   are set afterwards, and the next material); the percentages are normalised by their sum.
// - rfg_materials_system_init dereferences Rfg_physical_material_default even when no "default" material exists.

#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/destruction/rfg_shapes.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/geom/decal_info.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"

#include <new>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

rfg_effect_material* Rfg_effect_material_default; // 0x01EF82D0
unsigned char Rfg_effect_material_default_index = 0xFF; // 0x01650359
rfg_physical_material* Rfg_physical_material_default; // 0x01EF82CC
unsigned char Rfg_physical_material_default_index = 0xFF; // 0x01650358
unsigned char Rfg_melee_group_default_index = 0xFF; // 0x0165035A
unsigned char Rfg_bullet_group_default_index = 0xFF; // 0x0165035B
mempool Effect_material_pool; // 0x01F13D40
const char* Material_event_type_name[13] = { "explode", "load_balance", "stress", "detach", "groan", "shatter", "collapse", "melee", "bullet", "impact", "shift", "roll", "slide" }; // 0x0165035C
const char* Material_event_shape_type_name[5] = { "Solid", "Sheet", "Pole", "Fence", "Catwalk" }; // 0x01650390
const char* Material_flag_names[12] = { "shatter", "no_material_pickup", "indestructible", "bullet_damage", "melee_damage", "structural", "flammable", "transparent", "electrifiable", "crushable", "allow_jitter", "allow_regrow" }; // 0x016503B4

// ---- file-static globals
static int Num_rfg_effect_materials; // 0x01EF82B8
static rfg_effect_material* RFG_effect_materials; // 0x01E38324
static rfg_physical_material* RFG_physical_materials; // 0x01E38328
static int Num_rfg_physical_materials; // 0x01EF82B4
static int Num_rfg_melee_groups; // 0x01EF82C0
static char** RFG_melee_group_names; // 0x01EF82BC
static int Num_rfg_bullet_groups; // 0x01EF82C8
static char** RFG_bullet_group_names; // 0x01EF82C4
static char Effect_material_buffer[51200]; // 0x01E2BA50
static float Pole_scalar = 5.0f; // 0x016503E4
static float Sheet_scalar = 0.1f; // 0x016503E8
static bool Rfg_materials_system_init; // 0x01EF827F
static const char* pDefaultString = "Default"; // 0x016505B4, name as recorded by the analysis (placement inferred)

// 0x007104C0 (.h:211)
bool rfg_material_event_type_is_valid(material_event_type m)
{
    return (unsigned int)m <= MATERIAL_EVENT_SLIDE;
}

// 0x0071D1D0 (.h:213)
bool rfg_material_event_size_is_valid(material_event_size s)
{
    return (unsigned int)s <= MATERIAL_EVENT_SIZE_LARGE;
}

// 0x0071D1E0 (.h:214)
bool rfg_material_shape_type_is_valid(material_shape_type t)
{
    return (unsigned int)t <= MATERIAL_EVENT_SHAPE_CATWALK;
}

// 0x00716250 (:1812)
base_event_candidate::base_event_candidate()
{
    event_size[0] = 0.0f;
    event_size[1] = 0.0f;
    event_size[2] = 0.0f;
}

// 0x007104D0 (.h:230)
base_event_candidate::~base_event_candidate()
{
}

// 0x00716280 (:1825)
material_shape_type base_event_candidate::get_default_shape_type(material_shape_type material_default, unsigned char other_mat)
{
    // The first shape with a small event, or material_default when it has one.
    material_shape_type default_shape = INVALID_MATERIAL_EVENT_SHAPE_TYPE;
    for (int shape = MATERIAL_EVENT_SHAPE_SOLID; shape < NUM_MATERIAL_EVENT_SHAPE_TYPES; ++shape)
    {
        if (default_shape == INVALID_MATERIAL_EVENT_SHAPE_TYPE || shape == material_default)
        {
            if (get_event_handle((material_shape_type)shape, MATERIAL_EVENT_SIZE_SMALL, other_mat, 0xFF, 0xFF) != 0xFFFFFFFF)
            {
                default_shape = (material_shape_type)shape;
            }
        }
    }
    return default_shape;
}

// 0x0071D190 (:249)
single_event_candidate::single_event_candidate()
{
    memset(event_handle, 0xFF, sizeof(event_handle));
}

// 0x0071D250
single_event_candidate::~single_event_candidate()
{
}

// 0x0071D1F0 (:256)
unsigned int single_event_candidate::get_event_handle(material_shape_type shape_type, material_event_size size, unsigned char other_mat, unsigned char melee_group_index, unsigned char bullet_group_index)
{
    if ((unsigned int)shape_type < NUM_MATERIAL_EVENT_SHAPE_TYPES && (unsigned int)size < NUM_MATERIAL_EVENT_SIZES)
    {
        return event_handle[shape_type][size];
    }
    return 0xFFFFFFFF;
}

// 0x0071D220 (:267)
void single_event_candidate::set_event_handle(unsigned int handle, material_shape_type shape_type, material_event_size size, unsigned char other_mat, unsigned char melee_group_index, unsigned char bullet_group_index)
{
    if ((unsigned int)shape_type < NUM_MATERIAL_EVENT_SHAPE_TYPES && (unsigned int)size < NUM_MATERIAL_EVENT_SIZES)
    {
        event_handle[shape_type][size] = handle;
    }
}

// 0x0071D280 (:285)
pair_event_candidate::pair_event_candidate()
{
    // One optional handle block per other effect material.
    memset(default_handles, 0xFF, sizeof(default_handles));
    const int num_materials = Num_rfg_effect_materials;
    custom_handles = (unsigned int (**)[5][3])Effect_material_pool.static_mempool_base::alloc(num_materials * sizeof(*custom_handles), 0);
    if (custom_handles != nullptr)
    {
        memset(custom_handles, 0, num_materials * sizeof(*custom_handles));
    }
}

// 0x0071D3F0
pair_event_candidate::~pair_event_candidate()
{
}

// 0x0071D310 (:305)
unsigned int pair_event_candidate::get_event_handle(material_shape_type shape_type, material_event_size size, unsigned char other_mat, unsigned char melee_group_index, unsigned char bullet_group_index)
{
    if ((unsigned int)shape_type >= NUM_MATERIAL_EVENT_SHAPE_TYPES || (unsigned int)size >= NUM_MATERIAL_EVENT_SIZES)
    {
        return 0xFFFFFFFF;
    }
    if (other_mat != 0xFF && custom_handles[other_mat] != nullptr)
    {
        return (*custom_handles[other_mat])[shape_type][size];
    }
    return default_handles[shape_type][size];
}

// 0x0071D360 (:323)
void pair_event_candidate::set_event_handle(unsigned int handle, material_shape_type shape_type, material_event_size size, unsigned char other_mat, unsigned char melee_group_index, unsigned char bullet_group_index)
{
    if ((unsigned int)shape_type >= NUM_MATERIAL_EVENT_SHAPE_TYPES || (unsigned int)size >= NUM_MATERIAL_EVENT_SIZES)
    {
        return;
    }
    if (other_mat == 0xFF)
    {
        default_handles[shape_type][size] = handle;
        return;
    }
    if (custom_handles[other_mat] == nullptr)
    {
        custom_handles[other_mat] = (unsigned int (*)[5][3])Effect_material_pool.static_mempool_base::alloc(sizeof(**custom_handles), 0);
        memset(custom_handles[other_mat], 0xFF, sizeof(**custom_handles));
    }
    (*custom_handles[other_mat])[shape_type][size] = handle;
}

// 0x0071D480 (:344)
static bool rfg_material_melee_group_is_valid(unsigned char melee_group_index)
{
    return melee_group_index != 0xFF && melee_group_index < Num_rfg_melee_groups;
}

// 0x0071D420 (:354)
melee_event_candidate::melee_event_candidate()
{
    // One handle per melee group and shape (event sizes do not apply).
    const unsigned int size = Num_rfg_melee_groups * sizeof(*event_handle);
    event_handle = (unsigned int (*)[5])Effect_material_pool.static_mempool_base::alloc(size, 0);
    if (event_handle != nullptr)
    {
        memset(event_handle, 0xFF, size);
    }
}

// 0x0071D500
melee_event_candidate::~melee_event_candidate()
{
}

// 0x0071D4A0 (:365)
unsigned int melee_event_candidate::get_event_handle(material_shape_type shape_type, material_event_size size, unsigned char other_mat, unsigned char melee_group_index, unsigned char bullet_group_index)
{
    if ((unsigned int)shape_type < NUM_MATERIAL_EVENT_SHAPE_TYPES && rfg_material_melee_group_is_valid(melee_group_index))
    {
        return event_handle[melee_group_index][shape_type];
    }
    return 0xFFFFFFFF;
}

// 0x0071D4D0 (:376)
void melee_event_candidate::set_event_handle(unsigned int handle, material_shape_type shape_type, material_event_size size, unsigned char other_mat, unsigned char melee_group_index, unsigned char bullet_group_index)
{
    if ((unsigned int)shape_type < NUM_MATERIAL_EVENT_SHAPE_TYPES && rfg_material_melee_group_is_valid(melee_group_index))
    {
        event_handle[melee_group_index][shape_type] = handle;
    }
}

// 0x00715770 (:387)
static bool rfg_material_bullet_group_is_valid(unsigned char bullet_group_index)
{
    return bullet_group_index != 0xFF && bullet_group_index < Num_rfg_bullet_groups;
}

// 0x0071D530 (:400)
bullet_event_candidate::bullet_event_candidate()
{
    // One handle per bullet group and shape, and one decal info per bullet group.
    unsigned int size = Num_rfg_bullet_groups * sizeof(*event_handle);
    event_handle = (unsigned int (*)[5])Effect_material_pool.static_mempool_base::alloc(size, 0);
    if (event_handle != nullptr)
    {
        memset(event_handle, 0xFF, size);
        size = Num_rfg_bullet_groups * sizeof(*decal_info);
        decal_info = (int*)Effect_material_pool.static_mempool_base::alloc(size, 0);
        if (decal_info != nullptr)
        {
            memset(decal_info, 0xFF, size);
        }
    }
}

// 0x0071D620
bullet_event_candidate::~bullet_event_candidate()
{
}

// 0x0071D5C0 (:417)
unsigned int bullet_event_candidate::get_event_handle(material_shape_type shape_type, material_event_size size, unsigned char other_mat, unsigned char melee_group_index, unsigned char bullet_group_index)
{
    if ((unsigned int)shape_type < NUM_MATERIAL_EVENT_SHAPE_TYPES && rfg_material_bullet_group_is_valid(bullet_group_index))
    {
        return event_handle[bullet_group_index][shape_type];
    }
    return 0xFFFFFFFF;
}

// 0x0071D5F0 (:428)
void bullet_event_candidate::set_event_handle(unsigned int handle, material_shape_type shape_type, material_event_size size, unsigned char other_mat, unsigned char melee_group_index, unsigned char bullet_group_index)
{
    if ((unsigned int)shape_type < NUM_MATERIAL_EVENT_SHAPE_TYPES && rfg_material_bullet_group_is_valid(bullet_group_index))
    {
        event_handle[bullet_group_index][shape_type] = handle;
    }
}

// 0x00715790 (:437)
int bullet_event_candidate::get_bullet_decal_info(unsigned char bullet_group_index)
{
    if (rfg_material_bullet_group_is_valid(bullet_group_index))
    {
        return decal_info[bullet_group_index];
    }
    return -1;
}

// 0x007157C0 (:442)
void bullet_event_candidate::set_bullet_decal_info(int di, unsigned char bullet_group_index)
{
    // A group keeps its first decal.
    if (di != -1 && rfg_material_bullet_group_is_valid(bullet_group_index))
    {
        const int current = decal_info[bullet_group_index];
        if (current == -1 || current == di)
        {
            decal_info[bullet_group_index] = di;
        }
    }
}

// 0x0071D650 (:463)
static base_event_candidate* event_candidate_alloc(material_event_type type)
{
    // Candidates live in Effect_material_pool (never freed individually).
    switch (type)
    {
    case MATERIAL_EVENT_EXPLODE:
    case MATERIAL_EVENT_LOAD_BALANCE:
    case MATERIAL_EVENT_STRESS:
    case MATERIAL_EVENT_DETACH:
    case MATERIAL_EVENT_GROAN:
    case MATERIAL_EVENT_SHATTER:
    case MATERIAL_EVENT_COLLAPSE:
        if (Effect_material_pool.static_mempool_base::can_alloc(sizeof(single_event_candidate), 0))
        {
            void* memory = Effect_material_pool.static_mempool_base::alloc(sizeof(single_event_candidate), 0);
            if (memory != nullptr)
            {
                return new (memory) single_event_candidate();
            }
        }
        break;
    case MATERIAL_EVENT_MELEE:
        if (Effect_material_pool.static_mempool_base::can_alloc(sizeof(melee_event_candidate), 0))
        {
            void* memory = Effect_material_pool.static_mempool_base::alloc(sizeof(melee_event_candidate), 0);
            if (memory != nullptr)
            {
                return new (memory) melee_event_candidate();
            }
        }
        break;
    case MATERIAL_EVENT_BULLET:
        if (Effect_material_pool.static_mempool_base::can_alloc(sizeof(bullet_event_candidate), 0))
        {
            void* memory = Effect_material_pool.static_mempool_base::alloc(sizeof(bullet_event_candidate), 0);
            if (memory != nullptr)
            {
                return new (memory) bullet_event_candidate();
            }
        }
        break;
    case MATERIAL_EVENT_IMPACT:
    case MATERIAL_EVENT_SHIFT:
    case MATERIAL_EVENT_ROLL:
    case MATERIAL_EVENT_SLIDE:
        if (Effect_material_pool.static_mempool_base::can_alloc(sizeof(pair_event_candidate), 0))
        {
            void* memory = Effect_material_pool.static_mempool_base::alloc(sizeof(pair_event_candidate), 0);
            if (memory != nullptr)
            {
                return new (memory) pair_event_candidate();
            }
        }
        break;
    default:
        break;
    }
    return nullptr;
}

// 0x0071D740 (:505)
rfg_effect_material* rfg_get_effect_material(const char* name)
{
    const unsigned char index = rfg_get_effect_material_index(name);
    if (index != 0xFF && index < Num_rfg_effect_materials)
    {
        return &RFG_effect_materials[index];
    }
    return Rfg_effect_material_default;
}

// 0x00715800 (:512)
unsigned char rfg_get_effect_material_index(const char* name)
{
    // Case-sensitive; the default index when the name is unknown.
    if (name != nullptr)
    {
        for (int i = 0; i < Num_rfg_effect_materials; ++i)
        {
            if (strcmp(name, RFG_effect_materials[i].name) == 0)
            {
                return (unsigned char)i;
            }
        }
    }
    return Rfg_effect_material_default_index;
}

// 0x00715870 (:526)
unsigned char rfg_get_effect_material_index(rfg_effect_material* material)
{
    if (material == nullptr)
    {
        return 0xFF;
    }
    return (unsigned char)(material - RFG_effect_materials);
}

// 0x0071D770 (:532)
unsigned char rfg_get_effect_material_index(unsigned char mat_idx)
{
    // The effect material of a physical material.
    const rfg_physical_material* material = Rfg_physical_material_default;
    if (mat_idx != 0xFF && mat_idx < Num_rfg_physical_materials)
    {
        material = &RFG_physical_materials[mat_idx];
    }
    if (material->effect_material != nullptr)
    {
        return (unsigned char)(material->effect_material - RFG_effect_materials);
    }
    return 0xFF;
}

// 0x007158A0 (:539)
unsigned char rfg_get_melee_group_index(const char* name)
{
    if (name != nullptr)
    {
        for (int i = 0; i < Num_rfg_melee_groups; ++i)
        {
            if (strcmp(name, RFG_melee_group_names[i]) == 0)
            {
                return (unsigned char)i;
            }
        }
    }
    return Rfg_melee_group_default_index;
}

// 0x00715900 (:553)
unsigned char rfg_get_bullet_group_index(const char* name)
{
    if (name != nullptr)
    {
        for (int i = 0; i < Num_rfg_bullet_groups; ++i)
        {
            if (strcmp(name, RFG_bullet_group_names[i]) == 0)
            {
                return (unsigned char)i;
            }
        }
    }
    return Rfg_bullet_group_default_index;
}

// 0x00715960 (:570)
static void load_event_effects(xml_element* effect_element, base_event_candidate* candidate, material_shape_type shape_type, unsigned char other_mat, bool is_effect)
{
    // <Small>/<Medium>/<Large> name an effect (is_effect) or an explosion. Sizes left without one get the last one
    // found.
    unsigned int last_handle = 0xFFFFFFFF;
    for (xml_element* element = effect_element; element != nullptr; element = element->next)
    {
        material_event_size size;
        if (v_stricmp(element->name, "Small") == 0)
        {
            size = MATERIAL_EVENT_SIZE_SMALL;
        }
        else if (v_stricmp(element->name, "Medium") == 0)
        {
            size = MATERIAL_EVENT_SIZE_MEDIUM;
        }
        else if (v_stricmp(element->name, "Large") == 0)
        {
            size = MATERIAL_EVENT_SIZE_LARGE;
        }
        else
        {
            continue;
        }
        if (element->text == nullptr)
        {
            continue;
        }
        unsigned int handle;
        if (is_effect)
        {
            handle = effects_get_handle(element->text);
        }
        else
        {
            const explosion_info* explosion = explosion_lookup(element->text);
            if (explosion == nullptr)
            {
                continue;
            }
            handle = (unsigned int)(uintptr_t)explosion;
        }
        if (handle != 0xFFFFFFFF)
        {
            candidate->set_event_handle(handle, shape_type, size, other_mat, 0xFF, 0xFF);
            last_handle = handle;
        }
    }
    if (last_handle != 0xFFFFFFFF)
    {
        for (int size = MATERIAL_EVENT_SIZE_SMALL; size < NUM_MATERIAL_EVENT_SIZES; ++size)
        {
            if (candidate->get_event_handle(shape_type, (material_event_size)size, other_mat, 0xFF, 0xFF) == 0xFFFFFFFF)
            {
                candidate->set_event_handle(last_handle, shape_type, (material_event_size)size, other_mat, 0xFF, 0xFF);
            }
        }
    }
}

// 0x00715A70 (:629)
static void load_event_sizes(xml_element* size_element, base_event_candidate* candidate)
{
    for (; size_element != nullptr; size_element = size_element->next)
    {
        if (v_stricmp(size_element->name, "Small") == 0)
        {
            candidate->event_size[0] = (float)atof(size_element->text);
        }
        else if (v_stricmp(size_element->name, "Medium") == 0)
        {
            candidate->event_size[1] = (float)atof(size_element->text);
        }
        else if (v_stricmp(size_element->name, "Large") == 0)
        {
            candidate->event_size[2] = (float)atof(size_element->text);
        }
    }
}

// Copies the handle of default_shape to the shapes that have none, for every size (inlined in the original).
static void fill_shapes_from_default(base_event_candidate* candidate, material_shape_type default_shape, unsigned char other_mat)
{
    for (int shape = MATERIAL_EVENT_SHAPE_SOLID; shape < NUM_MATERIAL_EVENT_SHAPE_TYPES; ++shape)
    {
        for (int size = MATERIAL_EVENT_SIZE_SMALL; size < NUM_MATERIAL_EVENT_SIZES; ++size)
        {
            const unsigned int handle = candidate->get_event_handle(default_shape, (material_event_size)size, other_mat, 0xFF, 0xFF);
            if (handle != 0xFFFFFFFF && candidate->get_event_handle((material_shape_type)shape, (material_event_size)size, other_mat, 0xFF, 0xFF) == 0xFFFFFFFF)
            {
                candidate->set_event_handle(handle, (material_shape_type)shape, (material_event_size)size, other_mat, 0xFF, 0xFF);
            }
        }
    }
}

// 0x0071D7C0 (:650)
static void parse_rfg_effect_material_event_types_recursive(rfg_effect_material* effect_material, xml_element* event_xml)
{
    // Siblings are [<event_size>] [<default>] <event type>, repeated; "*_container" elements are descended into. An
    // event type already set, an allocation failure or a missing <Type> ends the whole parse of this material.
    for (; event_xml != nullptr; event_xml = event_xml->next)
    {
        if (strstr(event_xml->name, "_container") != nullptr)
        {
            parse_rfg_effect_material_event_types_recursive(effect_material, event_xml->elements);
            continue;
        }
        if (event_xml->elements == nullptr)
        {
            return;
        }
        xml_element* sizes = nullptr;
        if (v_stricmp(event_xml->name, "event_size") == 0)
        {
            sizes = event_xml->elements;
            event_xml = event_xml->next;
        }
        xml_element* defaults = nullptr;
        if (v_stricmp(event_xml->name, "default") == 0)
        {
            defaults = event_xml->elements;
            event_xml = event_xml->next;
        }
        unsigned char other_mat = 0xFF;
        int i; // the event type; reused by the default fill below (see the file comment)
        for (i = 0; i < NUM_MATERIAL_EVENT_TYPES; ++i)
        {
            if (v_stricmp(event_xml->name, Material_event_type_name[i]) == 0)
            {
                break;
            }
        }
        if (i == NUM_MATERIAL_EVENT_TYPES)
        {
            continue;
        }
        const material_event_type type = (material_event_type)i;
        if (effect_material->effect_candidate[type] != nullptr)
        {
            return;
        }
        base_event_candidate* candidate = event_candidate_alloc(type);
        if (candidate == nullptr)
        {
            return;
        }
        effect_material->effect_candidate[type] = candidate;
        if (sizes != nullptr)
        {
            load_event_sizes(sizes, candidate);
        }
        if (defaults != nullptr)
        {
            int default_shape = INVALID_MATERIAL_EVENT_SHAPE_TYPE;
            for (xml_element* entry = defaults; entry != nullptr; entry = entry->next)
            {
                xml_element* shape_element = xml_table_find_req(entry, "Type");
                if (shape_element == nullptr)
                {
                    return;
                }
                const material_shape_type shape = rfg_material_get_shape_type(shape_element->text);
                load_event_effects(entry->elements, candidate, shape, 0xFF, true);
                if (default_shape == INVALID_MATERIAL_EVENT_SHAPE_TYPE || shape == effect_material->default_shape_type)
                {
                    default_shape = shape;
                }
            }
            if (default_shape != INVALID_MATERIAL_EVENT_SHAPE_TYPE)
            {
                for (i = MATERIAL_EVENT_SHAPE_SOLID; i < NUM_MATERIAL_EVENT_SHAPE_TYPES; ++i)
                {
                    for (int size = MATERIAL_EVENT_SIZE_SMALL; size < NUM_MATERIAL_EVENT_SIZES; ++size)
                    {
                        if (candidate->get_event_handle((material_shape_type)i, (material_event_size)size, 0xFF, 0xFF, 0xFF) == 0xFFFFFFFF)
                        {
                            const unsigned int handle = candidate->get_event_handle((material_shape_type)default_shape, (material_event_size)size, 0xFF, 0xFF, 0xFF);
                            candidate->set_event_handle(handle, (material_shape_type)i, (material_event_size)size, 0xFF, 0xFF, 0xFF);
                        }
                    }
                }
            }
        }
        for (xml_element* entry = event_xml->elements; entry != nullptr; entry = entry->next)
        {
            xml_element* shape_element = xml_table_find_req(entry, "Type");
            if (shape_element == nullptr)
            {
                return;
            }
            const material_shape_type shape = rfg_material_get_shape_type(shape_element->text);
            if (type < NUM_MATERIAL_SINGLE_EVENT_TYPES)
            {
                load_event_effects(entry->elements, candidate, shape, other_mat, i != 0);
                continue;
            }
            xml_element* material_element = xml_table_find_req(entry, "Material");
            if (material_element == nullptr)
            {
                return;
            }
            other_mat = rfg_get_effect_material_index(material_element->text);
            if (other_mat != 0xFF)
            {
                load_event_effects(entry->elements, candidate, shape, other_mat, i != 0);
            }
            else
            {
                // Formatted into a scratch string only.
                char message[256];
                sprintf(message, "Invalid effect material event: %s - %s", effect_material->name, material_element->text);
            }
        }
        if (type < NUM_MATERIAL_SINGLE_EVENT_TYPES)
        {
            fill_shapes_from_default(candidate, candidate->get_default_shape_type(effect_material->default_shape_type, 0xFF), 0xFF);
        }
        else
        {
            for (int other = 0; other < Num_rfg_effect_materials; ++other)
            {
                fill_shapes_from_default(candidate, candidate->get_default_shape_type(effect_material->default_shape_type, (unsigned char)other), (unsigned char)other);
            }
        }
    }
}

// 0x0071DBD0 (:805)
static void parse_rfg_effect_material_event_types(rfg_effect_material* effect_material, xml_element* root)
{
    // Not used by rfg_materials_system_init, which inlines it.
    xml_element* event_effects = xml_find(root, "event_effects");
    if (event_effects != nullptr && root->elements != nullptr && strcmp(root->elements->text, effect_material->name) == 0)
    {
        parse_rfg_effect_material_event_types_recursive(effect_material, event_effects->elements);
    }
}

// 0x0071DC30 (:878)
static void parse_rfg_effect_material(rfg_effect_material* effect_material, xml_element* root)
{
    strncpy_safe(effect_material->name, xml_table_get_req_string_ref(root, "Name"), sizeof(effect_material->name));
    effect_material->default_shape_type = rfg_material_get_shape_type(xml_table_get_req_string_ref(root, "DefaultType"));

    // Effect tint: 0-255 components (missing ones 255), black and disabled without <Effect_tint>; stored linear.
    const rl_color black = rl_color::BLACK;
    unsigned char red = black.red;
    unsigned char green = black.green;
    unsigned char blue = black.blue;
    xml_element* tint = xml_find(root, "Effect_tint");
    if (tint == nullptr)
    {
        effect_material->effect_tint_enabled = false;
    }
    else
    {
        xml_element* red_element = xml_find(tint, "R");
        xml_element* green_element = xml_find(tint, "G");
        xml_element* blue_element = xml_find(tint, "B");
        red = 0xFF;
        green = 0xFF;
        blue = 0xFF;
        if (red_element != nullptr)
        {
            red = (unsigned char)atoi(red_element->text);
        }
        if (green_element != nullptr)
        {
            green = (unsigned char)atoi(green_element->text);
        }
        if (blue_element != nullptr)
        {
            blue = (unsigned char)atoi(blue_element->text);
        }
        effect_material->effect_tint_enabled = true;
    }
    effect_material->effect_tint.red = (float)((double)red / 255.0);
    effect_material->effect_tint.green = (float)((double)green / 255.0);
    effect_material->effect_tint.blue = (float)((double)blue / 255.0);
    effect_material->effect_tint.alpha = (float)((double)black.alpha / 255.0);
    effect_material->effect_tint = effect_material->effect_tint.from_sRGB_to_linear();

    effect_material->blast_decal = -1;
    const char* blast_decal = xml_table_get_opt_string_ref(root, "blast_decal");
    if (blast_decal != nullptr)
    {
        effect_material->blast_decal = decal_info_find(blast_decal);
    }
    xml_element* trail_effects = xml_table_find_req(root, "TrailEffects");
    effect_material->trail_effects[0] = effects_get_handle(xml_table_get_opt_string_ref(trail_effects, "DustTrail"));
    effect_material->trail_effects[1] = effects_get_handle(xml_table_get_opt_string_ref(trail_effects, "FireTrail"));

    for (int i = 0; i < 10; ++i)
    {
        effect_material->gust_effects[i].effect = 0xFFFFFFFF;
    }
    effect_material->num_gust_effects = 0;
    xml_element* gust_effects = xml_table_find_opt(root, "Gust_Effects");
    xml_element* effect_list = gust_effects != nullptr ? xml_table_find_opt(gust_effects, "Effect_List") : nullptr;
    if (effect_list != nullptr)
    {
        for (xml_element* entry = xml_table_find_opt(effect_list, "Effect_Element"); entry != nullptr && effect_material->num_gust_effects < 11; entry = xml_table_find_next(effect_list, entry, "Effect_Element"))
        {
            gust_effect_info& gust = effect_material->gust_effects[effect_material->num_gust_effects];
            gust.effect = effects_get_handle(xml_table_get_req_string_ref(entry, "Effect"));
            int percent_chance = 100;
            xml_table_get_req_int(percent_chance, entry, "Percent_Chance");
            float chance = (float)((double)(float)percent_chance / 100.0);
            if (chance - 0.0f < 0.0f)
            {
                chance = 0.0f;
            }
            if (chance - 1.0f >= 0.0f)
            {
                chance = 1.0f;
            }
            gust.percent_chance = chance;
            float fade_time = 0.0f;
            xml_table_get_req_float(fade_time, entry, "Fade_in_Time");
            gust.fade_time = fade_time;
            char near_far[8];
            xml_table_get_req_string(near_far, 5, entry, "Near_Far");
            gust.near_far = v_stricmp(near_far, "Near") == 0;
            ++effect_material->num_gust_effects;
        }
    }
    float total_chance = 0.0f;
    for (int i = 0; i < effect_material->num_gust_effects; ++i)
    {
        total_chance += effect_material->gust_effects[i].percent_chance;
    }
    for (int i = 0; i < effect_material->num_gust_effects; ++i)
    {
        effect_material->gust_effects[i].percent_chance = (float)((double)effect_material->gust_effects[i].percent_chance * (1.0 / (double)total_chance));
    }

    xml_element* driving_effects = xml_find(root, "driving_effects");
    effect_material->driving_do_tracks = false;
    effect_material->driving_front_effect = 0xFFFFFFFF;
    effect_material->driving_rear_effect = 0xFFFFFFFF;
    if (driving_effects != nullptr)
    {
        xml_table_get_opt_bool(effect_material->driving_do_tracks, driving_effects, "driving_tracks");
        xml_element* front = xml_find(driving_effects, "driving_front_effect");
        if (front != nullptr && front->text != nullptr)
        {
            effect_material->driving_front_effect = effects_get_handle(front->text);
        }
        xml_element* back = xml_find(driving_effects, "driving_back_effect");
        if (back != nullptr && back->text != nullptr)
        {
            effect_material->driving_rear_effect = effects_get_handle(back->text);
        }
    }
}

// 0x00715B10 (:1009)
static void parse_rfg_physical_material(rfg_physical_material* material, xml_element* root)
{
    const char* name = xml_table_get_req_string_ref(root, "Name");
    strncpy_safe(material->name, name, sizeof(material->name));
    material->name_checksum = crc_stri(name, 0);
    strncpy_safe(material->suffix, xml_table_get_req_string_ref(root, "Suffix"), sizeof(material->suffix));
    material->suffix_checksum = crc_stri(material->suffix, 0);
    const unsigned char effect_material = rfg_get_effect_material_index(xml_table_get_req_string_ref(root, "EffectMaterial"));
    material->effect_material = effect_material != 0xFF ? &RFG_effect_materials[effect_material] : nullptr;
    // The level object properties name is copied into Permanent_mempool.
    const char* base_props_name = xml_table_get_opt_string_ref(root, "LevelObjectProperties");
    if (base_props_name == nullptr)
    {
        base_props_name = pDefaultString;
    }
    material->base_props_name = (char*)Permanent_mempool.static_mempool_base::alloc((unsigned int)strlen(base_props_name) + 1, 1);
    strcpy(material->base_props_name, base_props_name);
    int link_strength;
    xml_table_get_req_int(link_strength, root, "LinkStrength");
    material->link_strength = (unsigned int)(link_strength * 1000);
    xml_table_get_req_float(material->density, root, "Density");
    material->brittleness = 1.0f;
    xml_table_get_opt_float(material->brittleness, root, "Brittleness");
    material->brittleness = (float)(1.0 / (double)material->brittleness);
    material->restitution = 0.05f;
    xml_table_get_opt_float(material->restitution, root, "Restitution");
    material->friction = 1.0f;
    xml_table_get_opt_float(material->friction, root, "Friction");
    xml_element* flags = xml_table_find_opt(root, "Flags");
    material->flags = flags != nullptr ? (unsigned int)xml_table_get_flags(Material_flag_names, 12, flags, true) : 0;
}

// 0x0071E0E0 (:1079)
static void melee_group_add_def(const char* mat_name, const char* shape_type_name, const char* effect_name, const unsigned char melee_group_index)
{
    // The first definition for a material sets all shapes; later ones override their own shape.
    rfg_effect_material* material = rfg_get_effect_material(mat_name);
    const unsigned int handle = effects_get_handle(effect_name);
    if (material->effect_candidate[MATERIAL_EVENT_MELEE] == nullptr)
    {
        material->effect_candidate[MATERIAL_EVENT_MELEE] = event_candidate_alloc(MATERIAL_EVENT_MELEE);
        if (material->effect_candidate[MATERIAL_EVENT_MELEE] == nullptr)
        {
            return;
        }
    }
    base_event_candidate* candidate = material->effect_candidate[MATERIAL_EVENT_MELEE];
    const material_shape_type shape = rfg_material_get_shape_type(shape_type_name);
    if (candidate->get_event_handle(shape, INVALID_MATERIAL_EVENT_SIZE, 0xFF, melee_group_index, 0xFF) != 0xFFFFFFFF)
    {
        candidate->set_event_handle(handle, shape, INVALID_MATERIAL_EVENT_SIZE, 0xFF, melee_group_index, 0xFF);
        return;
    }
    for (int i = MATERIAL_EVENT_SHAPE_SOLID; i < NUM_MATERIAL_EVENT_SHAPE_TYPES; ++i)
    {
        candidate->set_event_handle(handle, (material_shape_type)i, INVALID_MATERIAL_EVENT_SIZE, 0xFF, melee_group_index, 0xFF);
    }
}

// 0x0071E1E0 (:1108)
static void parse_rfg_melee_group(unsigned char group_index, xml_element* root)
{
    const char* name = xml_table_get_req_string_ref(root, "Name");
    RFG_melee_group_names[group_index] = (char*)Effect_material_pool.static_mempool_base::alloc((unsigned int)strlen(name) + 1, 0);
    if (RFG_melee_group_names[group_index] == nullptr)
    {
        return;
    }
    strcpy(RFG_melee_group_names[group_index], name);
    bool is_default = false;
    xml_table_get_req_bool(is_default, root, "Default");
    if (is_default)
    {
        Rfg_melee_group_default_index = group_index;
    }
    xml_element* effects = xml_table_find_req(root, "Effects");
    if (effects == nullptr)
    {
        return;
    }
    for (xml_element* group = xml_table_find_req(effects, "Group"); group != nullptr; group = xml_table_find_next(root, group, "Group"))
    {
        const char* mat_name = xml_table_get_req_string_ref(group, "Material");
        const char* shape_type_name = xml_table_get_req_string_ref(group, "Type");
        const char* effect_name = xml_table_get_req_string_ref(group, "Effect");
        melee_group_add_def(mat_name, shape_type_name, effect_name, group_index);
    }
}

// 0x0071E2E0 (:1148)
static void bullet_group_add_def(const char* mat_name, const char* shape_type_name, const char* effect_name, const char* decal_name, const unsigned char bullet_group_index)
{
    // As melee_group_add_def, plus the bullet hole decal of the group.
    const unsigned int handle = effects_get_handle(effect_name);
    const int decal = decal_info_find(decal_name);
    if (handle == 0xFFFFFFFF && decal == -1)
    {
        return;
    }
    rfg_effect_material* material = rfg_get_effect_material(mat_name);
    if (material->effect_candidate[MATERIAL_EVENT_BULLET] == nullptr)
    {
        material->effect_candidate[MATERIAL_EVENT_BULLET] = event_candidate_alloc(MATERIAL_EVENT_BULLET);
        if (material->effect_candidate[MATERIAL_EVENT_BULLET] == nullptr)
        {
            return;
        }
    }
    bullet_event_candidate* candidate = static_cast<bullet_event_candidate*>(material->effect_candidate[MATERIAL_EVENT_BULLET]);
    const material_shape_type shape = rfg_material_get_shape_type(shape_type_name);
    if (candidate->get_event_handle(shape, INVALID_MATERIAL_EVENT_SIZE, 0xFF, 0xFF, bullet_group_index) == 0xFFFFFFFF)
    {
        for (int i = MATERIAL_EVENT_SHAPE_SOLID; i < NUM_MATERIAL_EVENT_SHAPE_TYPES; ++i)
        {
            candidate->set_event_handle(handle, (material_shape_type)i, INVALID_MATERIAL_EVENT_SIZE, 0xFF, 0xFF, bullet_group_index);
        }
    }
    else
    {
        candidate->set_event_handle(handle, shape, INVALID_MATERIAL_EVENT_SIZE, 0xFF, 0xFF, bullet_group_index);
    }
    candidate->set_bullet_decal_info(decal, bullet_group_index); // inlined in the original
}

// 0x0071E420 (:1188)
static void parse_rfg_bullet_group(unsigned char group_index, xml_element* root)
{
    const char* name = xml_table_get_req_string_ref(root, "Name");
    RFG_bullet_group_names[group_index] = (char*)Effect_material_pool.static_mempool_base::alloc((unsigned int)strlen(name) + 1, 0);
    if (RFG_bullet_group_names[group_index] == nullptr)
    {
        return;
    }
    strcpy(RFG_bullet_group_names[group_index], name);
    bool is_default = false;
    xml_table_get_req_bool(is_default, root, "Default");
    if (is_default)
    {
        Rfg_bullet_group_default_index = group_index;
    }
    xml_element* effects = xml_table_find_req(root, "Effects");
    if (effects == nullptr)
    {
        return;
    }
    for (xml_element* group = xml_table_find_req(effects, "Group"); group != nullptr; group = xml_table_find_next(root, group, "Group"))
    {
        const char* mat_name = xml_table_get_req_string_ref(group, "Material");
        const char* shape_type_name = xml_table_get_req_string_ref(group, "Type");
        const char* effect_name = xml_table_get_opt_string_ref(group, "Effect");
        const char* decal_name = xml_table_get_opt_string_ref(group, "Bullet_Hole");
        bullet_group_add_def(mat_name, shape_type_name, effect_name, decal_name, group_index);
    }
}

// 0x007252F0 (:1236)
void rfg_materials_system_init(bool ignore_max_only_materials, const char* pDataSourcePath)
{
    // pDataSourcePath is unused. With ignore_max_only_materials, materials marked NoShowMax are left out.
    if (Rfg_materials_system_init)
    {
        return;
    }
    Rfg_materials_system_init = true;
    Effect_material_pool.init(Effect_material_buffer, sizeof(Effect_material_buffer), "Effect candidate event pool", 4, 0);

    xml_element* root = xml_table_parse("rfg_effect_materials.xtbl");
    Num_rfg_effect_materials = xml_table_count(root, "Material");
    RFG_effect_materials = (rfg_effect_material*)Effect_material_pool.static_mempool_base::alloc(Num_rfg_effect_materials * sizeof(rfg_effect_material), 0);
    if (RFG_effect_materials == nullptr)
    {
        xml_table_close(root);
        return;
    }
    rfg_effect_material* effect_material = RFG_effect_materials;
    for (xml_element* element = xml_table_find_opt(root, "Material"); element != nullptr; element = xml_table_find_next(root, element, "Material"), ++effect_material)
    {
        parse_rfg_effect_material(effect_material, element);
    }
    // Event effects are parsed once all materials exist (pair events name other materials).
    effect_material = RFG_effect_materials;
    for (xml_element* element = xml_table_find_opt(root, "Material"); element != nullptr; element = xml_table_find_next(root, element, "Material"), ++effect_material)
    {
        parse_rfg_effect_material_event_types(effect_material, element); // inlined in the original
    }
    xml_table_close(root);

    root = xml_table_parse("rfg_materials.xtbl");
    Num_rfg_physical_materials = xml_table_count(root, "Material");
    RFG_physical_materials = (rfg_physical_material*)Effect_material_pool.static_mempool_base::alloc(Num_rfg_physical_materials * sizeof(rfg_physical_material), 0);
    if (RFG_physical_materials != nullptr)
    {
        rfg_physical_material* material = RFG_physical_materials;
        for (xml_element* element = xml_table_find_opt(root, "Material"); element != nullptr; element = xml_table_find_next(root, element, "Material"))
        {
            bool no_show_max = false;
            xml_table_get_req_bool(no_show_max, element, "NoShowMax");
            if (no_show_max && ignore_max_only_materials)
            {
                --Num_rfg_physical_materials;
            }
            else
            {
                parse_rfg_physical_material(material++, element);
            }
        }
        Rfg_physical_material_default_index = rfg_get_physical_material_index("default");
        if (Rfg_physical_material_default_index != 0xFF && Rfg_physical_material_default_index < Num_rfg_physical_materials)
        {
            Rfg_physical_material_default = &RFG_physical_materials[Rfg_physical_material_default_index];
        }
        if (Rfg_physical_material_default->effect_material == nullptr)
        {
            Rfg_effect_material_default_index = 0xFF;
        }
        else
        {
            Rfg_effect_material_default_index = (unsigned char)(Rfg_physical_material_default->effect_material - RFG_effect_materials);
            if (Rfg_effect_material_default_index != 0xFF && Rfg_effect_material_default_index < Num_rfg_effect_materials)
            {
                Rfg_effect_material_default = &RFG_effect_materials[Rfg_effect_material_default_index];
            }
        }
        xml_table_close(root);

        root = xml_table_parse("rfg_melee_groups.xtbl");
        Num_rfg_melee_groups = xml_table_count(root, "Melee_Group");
        RFG_melee_group_names = (char**)Effect_material_pool.static_mempool_base::alloc(Num_rfg_melee_groups * sizeof(char*), 0);
        if (RFG_melee_group_names == nullptr)
        {
            Num_rfg_melee_groups = 0;
        }
        else
        {
            unsigned char group_index = 0;
            for (xml_element* element = xml_table_find_opt(root, "Melee_Group"); element != nullptr; element = xml_table_find_next(root, element, "Melee_Group"))
            {
                parse_rfg_melee_group(group_index++, element);
            }
            xml_table_close(root);

            root = xml_table_parse("rfg_bullet_groups.xtbl");
            Num_rfg_bullet_groups = xml_table_count(root, "Bullet_Group");
            RFG_bullet_group_names = (char**)Effect_material_pool.static_mempool_base::alloc(Num_rfg_bullet_groups * sizeof(char*), 0);
            if (RFG_bullet_group_names == nullptr)
            {
                Num_rfg_bullet_groups = 0;
            }
            else
            {
                group_index = 0;
                for (xml_element* element = xml_table_find_opt(root, "Bullet_Group"); element != nullptr; element = xml_table_find_next(root, element, "Bullet_Group"))
                {
                    parse_rfg_bullet_group(group_index++, element);
                }
            }
        }
    }
    xml_table_close(root);
}

// 0x00715CD0 (:1439)
void rfg_materials_system_shutdown()
{
    if (Rfg_materials_system_init)
    {
        Rfg_materials_system_init = false;
        Num_rfg_physical_materials = 0;
        Num_rfg_effect_materials = 0;
        Effect_material_pool.clear();
    }
}

// 0x00715D00 (:1455)
int rfg_get_physical_material_count()
{
    return Num_rfg_physical_materials;
}

// 0x00715D10 (:1462)
int rfg_get_effect_material_count()
{
    return Num_rfg_effect_materials;
}

// 0x00715D20 (:1468)
int rfg_get_melee_group_count()
{
    return Num_rfg_melee_groups;
}

// 0x00715D30 (:1474)
int rfg_get_bullet_group_count()
{
    return Num_rfg_bullet_groups;
}

// 0x00715D40 (:1480)
bool rfg_physical_material_index_is_valid(unsigned char i)
{
    return i != 0xFF && i < Num_rfg_physical_materials;
}

// 0x00715D60 (:1486)
bool rfg_is_same_material(unsigned char idx, const char* name)
{
    return rfg_physical_material_index_is_valid(idx) && RFG_physical_materials[idx].name_checksum == crc_stri(name, 0);
}

// 0x00715DA0 (:1492)
bool rfg_effect_material_index_is_valid(unsigned char i)
{
    return i != 0xFF && i < Num_rfg_effect_materials;
}

// 0x00715DC0 (:1498)
bool rfg_is_same_effect_material(unsigned char idx, const char* name)
{
    if (!rfg_effect_material_index_is_valid(idx))
    {
        return false;
    }
    return crc_stri(RFG_effect_materials[idx].name, 0) == crc_stri(name, 0);
}

// 0x00715E10 (:1514)
rfg_physical_material* rfg_get_physical_material_by_index(unsigned char index)
{
    return rfg_physical_material_index_is_valid(index) ? &RFG_physical_materials[index] : Rfg_physical_material_default;
}

// 0x00715E40 (:1525)
rfg_effect_material* rfg_get_effect_material_by_index(unsigned char index)
{
    return rfg_effect_material_index_is_valid(index) ? &RFG_effect_materials[index] : Rfg_effect_material_default;
}

// 0x00715EA0 (:1585)
unsigned char rfg_get_physical_material_index_by_checksum(unsigned int name_checksum)
{
    for (int i = 0; i < Num_rfg_physical_materials; ++i)
    {
        if (RFG_physical_materials[i].name_checksum == name_checksum)
        {
            return (unsigned char)i;
        }
    }
    return Rfg_physical_material_default_index;
}

// 0x0071E570 (:1572)
rfg_physical_material* rfg_get_physical_material_by_checksum(unsigned int name_checksum)
{
    return rfg_get_physical_material_by_index(rfg_get_physical_material_index_by_checksum(name_checksum));
}

// 0x0071E530 (:1554)
unsigned char rfg_get_physical_material_index(const char* name)
{
    return rfg_get_physical_material_index_by_checksum(crc_stri(name, 0));
}

// 0x007257B0 (:1540)
rfg_physical_material* rfg_get_physical_material(const char* name)
{
    return rfg_get_physical_material_by_index(rfg_get_physical_material_index(name));
}

// 0x00715E70 (:1560)
unsigned char rfg_get_physical_material_index(rfg_physical_material* material)
{
    if (material == nullptr)
    {
        return 0xFF;
    }
    return (unsigned char)(material - RFG_physical_materials);
}

// 0x00715EE0 (:1607)
unsigned char rfg_get_physical_material_index_by_suffix(const char* suffix)
{
    const unsigned int suffix_checksum = crc_stri(suffix, 0);
    for (int i = 0; i < Num_rfg_physical_materials; ++i)
    {
        if (RFG_physical_materials[i].suffix_checksum == suffix_checksum)
        {
            return (unsigned char)i;
        }
    }
    return Rfg_physical_material_default_index;
}

// 0x0071E5C0 (:1600)
rfg_physical_material* rfg_get_physical_material_by_suffix(const char* suffix)
{
    return rfg_get_physical_material_by_index(rfg_get_physical_material_index_by_suffix(suffix));
}

// 0x00715F20 (:1621)
const char* rfg_physical_material_get_suffix(unsigned char mat_index)
{
    rfg_physical_material* material = rfg_get_physical_material_by_index(mat_index);
    return material != nullptr ? material->suffix : nullptr;
}

// 0x00715F50 (:1633)
material_event_category rfg_material_category_from_type(material_event_type event_type)
{
    switch (event_type)
    {
    case MATERIAL_EVENT_EXPLODE:
    case MATERIAL_EVENT_LOAD_BALANCE:
        return MATERIAL_EVENT_CATEGORY_LOAD_BALANCE;
    case MATERIAL_EVENT_STRESS:
    case MATERIAL_EVENT_DETACH:
    case MATERIAL_EVENT_GROAN:
        return MATERIAL_EVENT_CATEGORY_STRESS;
    case MATERIAL_EVENT_SHATTER:
    case MATERIAL_EVENT_COLLAPSE:
    case MATERIAL_EVENT_MELEE:
    case MATERIAL_EVENT_BULLET:
    case MATERIAL_EVENT_IMPACT:
    case MATERIAL_EVENT_SHIFT:
    case MATERIAL_EVENT_ROLL:
    case MATERIAL_EVENT_SLIDE:
        return MATERIAL_EVENT_CATEGORY_COLLISION;
    default:
        return MATERIAL_EVENT_CATEGORY_DEFAULT;
    }
}

// 0x00715FA0 (:1662)
material_shape_type rfg_material_get_default_shape_type(unsigned char mat_index)
{
    return rfg_get_physical_material_by_index(mat_index)->effect_material->default_shape_type;
}

// 0x00715FD0 (:1677)
material_shape_type rfg_material_get_shape_type(const vector& bmin, const vector& bmax)
{
    // Sorted extents: a pole when the largest is over 5 times the middle one, a sheet when the smallest is under 0.1
    // times the middle one.
    float x = bmax.x - bmin.x;
    float y = bmax.y - bmin.y;
    const float z = bmax.z - bmin.z;
    float smaller = x;
    float larger = y;
    if (x - y >= 0.0f)
    {
        smaller = y;
        larger = x;
    }
    const float largest = larger - z >= 0.0f ? larger : z;
    float middle = larger;
    if (z <= larger)
    {
        middle = smaller - z >= 0.0f ? smaller : z;
    }
    const float smallest = smaller - z < 0.0f ? smaller : z;
    if (Pole_scalar * middle < largest)
    {
        return MATERIAL_EVENT_SHAPE_POLE;
    }
    if (smallest < Sheet_scalar * middle)
    {
        return MATERIAL_EVENT_SHAPE_SHEET;
    }
    return MATERIAL_EVENT_SHAPE_SOLID;
}

// 0x00725810 (:1710)
// The shape type of a piece of an rfg_mover (cached in its base, shape_type -1 until known; unloaded pieces have
// none) or of the object's bounding box.
material_shape_type rfg_material_get_shape_type(const object* objp, unsigned int shape_key)
{
    const rfg_mover* rm = objp != nullptr && objp->get_obj_type() == OT_MOVER && objp->get_sub_type() == OT_SUB_MOVER_RFG ? (const rfg_mover*)objp : nullptr;
    rfg_subpiece_base* sp_base = nullptr;
    vector bmin;
    vector bmax;
    if (rm == nullptr || shape_key == 0xFFFFFFFF)
    {
        objp->get_local_bbox(bmin, bmax); // not checked
    }
    else
    {
        const rfg_destroyable_shape* shape = rm->rdd.is_loaded() ? rm->rdd.get_data()->shape : nullptr;
        if (shape == nullptr || (sp_base = shape->get_subpiece_base(shape_key)) == nullptr)
        {
            return INVALID_MATERIAL_EVENT_SHAPE_TYPE;
        }
        if (sp_base->shape_type != -1)
        {
            return (material_shape_type)sp_base->shape_type;
        }
        bmin = sp_base->prel_bmin;
        bmax = sp_base->prel_bmax;
    }
    const material_shape_type type = rfg_material_get_shape_type(bmin, bmax); // inlined
    if (sp_base != nullptr)
    {
        sp_base->shape_type = (signed char)type;
    }
    return type;
}

// 0x00716110 (:1750)
material_shape_type rfg_material_get_shape_type(const char* name)
{
    if (name != nullptr)
    {
        for (int i = MATERIAL_EVENT_SHAPE_SOLID; i < NUM_MATERIAL_EVENT_SHAPE_TYPES; ++i)
        {
            if (strcmp(name, Material_event_shape_type_name[i]) == 0)
            {
                return (material_shape_type)i;
            }
        }
    }
    return INVALID_MATERIAL_EVENT_SHAPE_TYPE;
}

// 0x00716170 (:1765)
const char* rfg_material_get_shape_type_name(material_shape_type type)
{
    if ((unsigned int)type < NUM_MATERIAL_EVENT_SHAPE_TYPES)
    {
        return Material_event_shape_type_name[type];
    }
    return "Invalid shape type";
}

// 0x00716190 (:1776)
int rfg_effect_material_get_event_handle(material_event_type type, material_event_size size, material_shape_type shape_type, unsigned char mat_index, unsigned char other_mat_index, unsigned char melee_group_index)
{
    rfg_effect_material* material = rfg_get_effect_material_by_index(mat_index);
    if ((unsigned int)type < NUM_MATERIAL_EVENT_TYPES && material->effect_candidate[type] != nullptr)
    {
        return (int)material->effect_candidate[type]->get_event_handle(shape_type, size, other_mat_index, melee_group_index, 0xFF);
    }
    return -1;
}

// 0x007161F0 (:1797)
unsigned int rfg_effect_material::get_bullet_impact_effect(material_shape_type shape_type, unsigned char bullet_group_index)
{
    if (effect_candidate[MATERIAL_EVENT_BULLET] != nullptr)
    {
        return effect_candidate[MATERIAL_EVENT_BULLET]->get_event_handle(shape_type, INVALID_MATERIAL_EVENT_SIZE, 0xFF, 0xFF, bullet_group_index);
    }
    return 0xFFFFFFFF;
}

// 0x00716220 (:1803)
int rfg_effect_material::get_bullet_impact_decal_info(unsigned char bullet_group_index)
{
    if (effect_candidate[MATERIAL_EVENT_BULLET] != nullptr)
    {
        return static_cast<bullet_event_candidate*>(effect_candidate[MATERIAL_EVENT_BULLET])->get_bullet_decal_info(bullet_group_index);
    }
    return -1;
}
