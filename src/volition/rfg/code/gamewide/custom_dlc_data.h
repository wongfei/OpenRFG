// Original: volition/rfg/code/gamewide/custom_dlc_data.h
// Implementation: volition/rfg/code/gamewide/custom_dlc_data.cpp
//
// Custom DLC values: gameplay variables (bool, int, float) registered by name with the variable they override; a DLC
// level's custom data table assigns new values on level init (custom_dlc_data_level_init) and the variables are
// reset on level shutdown.
#pragma once


struct xml_element;

// size 0xC
class custom_dlc_value
{
public:
    custom_dlc_value(const char* in_name); // 0x007E8BB0 (:95)
    bool is_matching_name(const char* test_value_name, unsigned int test_value_crc) const; // 0x007B9D90 (:114)
    virtual void reset() = 0; // vtbl+0x00
    virtual void assign_new_value(xml_element*) = 0; // vtbl+0x04

    // 0x00 vfptr
private:
    const char* value_name; // 0x04
    unsigned int value_crc; // 0x08
};

// size 0x14
class custom_dlc_bool : public custom_dlc_value
{
public:
    custom_dlc_bool(const char* in_name, bool& in_value); // 0x007E8C30 (:131)
    virtual void reset(); // vtbl+0x00 0x007B9DC0 (:137)
    virtual void assign_new_value(xml_element* xml_value); // vtbl+0x04 0x007B9DD0 (:144)

private:
    const bool original_in_value; // 0x0C
    bool* value_ptr; // 0x10
};

// size 0x14
class custom_dlc_int : public custom_dlc_value
{
public:
    custom_dlc_int(const char* in_name, int& in_value); // 0x007E8C60 (:169)
    virtual void reset(); // vtbl+0x00 0x007B9E20 (:175)
    virtual void assign_new_value(xml_element* xml_value); // vtbl+0x04 0x007B9E30 (:182)

private:
    const int original_in_value; // 0x0C
    int* value_ptr; // 0x10
};

// size 0x14
class custom_dlc_float : public custom_dlc_value
{
public:
    custom_dlc_float(const char* in_name, float& in_value); // 0x007E8C90 (:206)
    virtual void reset(); // vtbl+0x00 0x007B9E70 (:212)
    virtual void assign_new_value(xml_element* xml_value); // vtbl+0x04 0x007B9E80 (:219)

private:
    const float original_in_value; // 0x0C
    float* value_ptr; // 0x10
};

void custom_dlc_data_level_init(int dlc_bundle_id); // 0x007ECDA0 (:236)
void custom_dlc_data_level_shutdown(); // 0x007E8CC0 (:272)
