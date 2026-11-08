// Original: volition/rfg/code/weather/weather.cpp
//
// The weather system (decompiled; header comment in weather.h): weather.xtbl's wind ranges and dust settings, the
// per-frame wind and dust update, and the level hooks (the dust's, inlined by LTCG).
//
// Split TU (VFX.md 2.6): compiled twice. The rfg library builds only Weather_stats, which the undergrowth's wind reads
// in every program (the state stays at no wind without the effect library). The effect library (rfg_effects,
// OPENRFG_EFFECTS) builds the system; the programs without it keep its stand-ins.

#include "volition/rfg/code/weather/weather.h"

#ifndef OPENRFG_EFFECTS

rfg_weather_info Weather_stats; // 0x038B2F90

#else // OPENRFG_EFFECTS

#include "volition/rfg/code/code_effects/dust.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/parse/xml_table_tools.h"

// ---- file-static globals
static bool Weather_init; // 0x038B30BC

// 0x00BECE20 (:42)
// The Weather element's Wind and Dust children.
bool weather_parse_xtbl(const char* filename)
{
    if (filename == nullptr)
    {
        return false;
    }
    xml_element* root = xml_table_parse(filename);
    if (root != nullptr)
    {
        xml_element* weather = xml_find(root, "Weather");
        if (weather != nullptr)
        {
            xml_element* wind = xml_find(weather, "Wind");
            if (wind != nullptr)
            {
                weather_internal_parse_wind(wind);
            }
            xml_element* dust = xml_find(weather, "Dust");
            if (dust != nullptr)
            {
                weather_internal_parse_dust(dust);
            }
        }
        xml_table_close(root);
    }
    return true;
}

// 0x00BEC630 (:73)
void weather_level_init_pre()
{
    dust_level_init_pre();
}

// 0x00BEC640 (:79)
void weather_level_shutdown()
{
    dust_level_shutdown();
}

// 0x00BECEA0 (:85)
bool weather_system_init()
{
    if (!Weather_init)
    {
        Weather_init = true;
        if (!weather_parse_xtbl("weather.xtbl"))
        {
            return false;
        }
        dust_system_init();
    }
    return true;
}

// 0x00BEC650 (:103)
void weather_system_shutdown()
{
    if (Weather_init)
    {
        Weather_init = false;
        dust_system_shutdown();
    }
}

// 0x00BECED0 (:115)
void weather_update()
{
    weather_wind_update();
    dust_update();
}

#endif // OPENRFG_EFFECTS
