// Original: volition/rfg/code/code_effects/glare.cpp
//
// Partial (stub_impl.md S3): the glare table (glares.xtbl into Glare_info), the free/used lists over Glare_buf and the
// small queries. Creating, attaching and rendering glares (glare_create*, glare_render_one/all, valid_los) and the
// console commands are not ported; nothing in the port creates a glare yet.

#include "volition/rfg/code/code_effects/glare.h"

#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"

static glare* Glare_free_list; // 0x01DEE420
static glare* Glare_used_list; // 0x01DEE478
static int Glares_inited; // 0x01DEE49C
static glare Glare_buf[256]; // 0x01DEE4F0
int Num_glare_types; // 0x01DEE498
glare_info Glare_info[64]; // 0x01E1A770

// 0x006E2290
glare_info::glare_info()
{
}

// 0x006E2280
glare::glare()
{
}

// Appends gp to the circular list whose head is list (the inlined list add of glare_deallocate and glare_flush_all).
static void glare_list_append(glare*& list, glare* gp)
{
    if (list == nullptr)
    {
        gp->next = gp;
        gp->prev = gp;
        list = gp;
        return;
    }
    gp->prev = list->prev;
    gp->next = list;
    list->prev->next = gp;
    list->prev = gp;
}

// 0x006E22E0 (:102)
static void glare_deallocate(glare* gp)
{
    if (Glare_used_list == gp && (Glare_used_list = gp->next) == gp)
    {
        Glare_used_list = nullptr;
    }
    else
    {
        if (gp->prev != nullptr)
        {
            gp->prev->next = gp->next;
        }
        if (gp->next != nullptr)
        {
            gp->next->prev = gp->prev;
        }
    }
    gp->prev = nullptr;
    gp->next = nullptr;
    glare_list_append(Glare_free_list, gp);
}

// 0x006E2350 (:108)
// The head of the free list moved to the end of the used list; null when all 256 are in use.
static glare* glare_allocate()
{
    glare* gp = Glare_free_list;
    if (gp == nullptr)
    {
        return nullptr;
    }
    if (gp->next == gp)
    {
        Glare_free_list = nullptr;
    }
    else
    {
        Glare_free_list = gp->next;
        if (gp->prev != nullptr)
        {
            gp->prev->next = gp->next;
        }
        if (gp->next != nullptr)
        {
            gp->next->prev = gp->prev;
        }
    }
    gp->prev = nullptr;
    gp->next = nullptr;
    glare_list_append(Glare_used_list, gp);
    return gp;
}

// 0x006E23D0 (:204)
// One <Glare> of glares.xtbl into Glare_info[Num_glare_types] (the caller counts it). Without a Fixed_Radius the
// radius varies with distance; a Volumetric_Bitmap adds a light cone (angle = half the cone angle, 180 without one).
static void parse_glare(xml_element* root)
{
    glare_info& info = Glare_info[Num_glare_types];
    xml_table_get_req_string(info.name, sizeof(info.name), root, "Name");
    xml_element* light_color = xml_table_find_req(root, "Light_Color");
    int r;
    int g;
    int b;
    int a;
    xml_table_get_req_int(r, light_color, "R");
    xml_table_get_req_int(g, light_color, "G");
    xml_table_get_req_int(b, light_color, "B");
    xml_table_get_req_int(a, light_color, "A");
    info.light_color.red = (unsigned char)r;
    info.light_color.green = (unsigned char)g;
    info.light_color.blue = (unsigned char)b;
    info.light_color.alpha = (unsigned char)a;
    char filename[128];
    xml_element* corona = xml_table_find_req(root, "Corona_Bitmap");
    xml_table_get_req_filename(filename, sizeof(filename), corona, "Bitmap_File");
    info.glare_bitmap = bm_load(filename, false);
    xml_table_get_req_bool(info.view_corona_from_rear, corona, "Rear_View");
    xml_table_get_req_float(info.intensity, corona, "Intensity");
    xml_element* radius = xml_table_find_req(corona, "Radius");
    xml_element* fixed = xml_table_find_opt(radius, "Fixed_Radius");
    if (fixed == nullptr)
    {
        xml_element* variable = xml_table_find_req(radius, "Variable_Radius");
        xml_table_get_req_float(info.max_radius, variable, "Max");
        xml_table_get_req_float(info.radius_dist_factor, variable, "Distance_Factor");
        xml_table_get_req_float(info.radius_scale_factor, variable, "Scale_Factor");
        xml_table_get_req_float(info.diminish_distance, variable, "Diminish_Distance");
        info.fixed_radius = 0.0f;
    }
    else
    {
        xml_table_get_req_float(info.fixed_radius, fixed, nullptr);
        info.max_radius = info.fixed_radius;
    }
    info.angle = 180.0f;
    info.volumetric_bitmap = -1;
    xml_element* volumetric = xml_table_find_opt(root, "Volumetric_Bitmap");
    if (volumetric != nullptr)
    {
        xml_table_get_req_filename(filename, sizeof(filename), volumetric, "Bitmap_File");
        info.volumetric_bitmap = bm_load(filename, false);
        float cone_angle;
        xml_table_get_req_float(cone_angle, volumetric, "Cone_Angle");
        info.angle = cone_angle * 0.5f;
        xml_table_get_req_float(info.volumetric_height, volumetric, "Volumetric_Height");
        xml_table_get_req_float(info.volumetric_length, volumetric, "Volumetric_Length");
        info.volumetric_intensity = 1.0f;
    }
}

// 0x006E2D90 (:502)
// Every glare back on the free list, in buffer order.
static void glare_flush_all()
{
    Glare_free_list = nullptr;
    Glare_used_list = nullptr;
    for (int i = 0; i < 256; i++)
    {
        glare_list_append(Glare_free_list, &Glare_buf[i]);
    }
}

// 0x006E2E60 (:515)
// The largest of the corona radius and, with a light cone, its height and length.
float glare_calculate_cull_radius(int info_index)
{
    const glare_info& info = Glare_info[info_index];
    if (info.volumetric_bitmap < 0)
    {
        return info.max_radius;
    }
    float radius = info.volumetric_length;
    if (info.volumetric_length - info.volumetric_height < 0.0f)
    {
        radius = info.volumetric_height;
    }
    if (radius - info.max_radius < 0.0f)
    {
        return info.max_radius;
    }
    return radius;
}

// 0x006E87D0 (:536)
void glare_system_init()
{
    Num_glare_types = 0;
    glare_parse_table();
    glare_flush_all();
    Glares_inited = 1;
}

// 0x006E2ED0 (:551)
void glare_level_init_pre()
{
    glare_flush_all(); // inlined
}

// 0x006E2EE0 (:556)
void glare_parse_table()
{
    Num_glare_types = 0;
    xml_element* root = xml_table_parse("glares.xtbl");
    // original: no check against the 64 entries of Glare_info
    for (xml_element* element = xml_table_find_opt(root, "Glare"); element != nullptr; element = xml_table_find_next(root, element, "Glare"))
    {
        parse_glare(element); // inlined
        Num_glare_types++;
    }
    xml_table_close(root);
}

// 0x006E34E0 (:703)
void glare_delete(glare* gp)
{
    glare_deallocate(gp);
}

// 0x006E3540 (:728)
int glare_info_lookup(const char* name)
{
    for (int i = 0; i < Num_glare_types; i++)
    {
        if (v_stricmp(name, Glare_info[i].name) == 0)
        {
            return i;
        }
    }
    return -1;
}

// 0x006E3680 (:800)
void glare_turn_on(glare* gp)
{
    gp->flags |= 1; // bit 0: on
}

// 0x006E3690 (:805)
void glare_turn_off(glare* gp)
{
    gp->flags &= ~1;
}
