// Original: volition/rfg/code/multi/multi_game/multi_game_overrides.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_overrides.cpp
//
// Multiplayer overrides of single player tables: backpack, explosion and weapon values replaced while a multiplayer game
// runs and restored afterwards (override_val<T> keeps the original).
#pragma once


struct explosion_info;
class mempool_base;
struct multi_backpack_info;
struct weapon_info;

// size (<int>) 0xC
template<typename T>
class override_val
{
public:
    void set_overrride_val(T);
    void apply_override(T&);
    void recover_override(T&);
    T get_override_val();

private:
    T o_val; // 0x00
    T base_val; // 0x04
    bool in_use; // 0x08
    bool override_set; // 0x09
};

// <float> 0x00968030 (:51): the override value, used from now on
template<typename T>
void override_val<T>::set_overrride_val(T _val)
{
    in_use = true;
    o_val = _val;
}

// <float> 0x00968050 (:52): the value is kept and replaced
template<typename T>
void override_val<T>::apply_override(T& val)
{
    if (in_use)
    {
        base_val = val;
        val = o_val;
        override_set = true;
    }
}

// <float> 0x00968070 (:53): the kept value comes back
template<typename T>
void override_val<T>::recover_override(T& val)
{
    if (in_use)
    {
        val = base_val;
        override_set = false;
    }
}

template<typename T>
T override_val<T>::get_override_val()
{
    return o_val;
}

// size 0x38
struct backpack_override_info
{
    void init(); // 0x0095F260 (:67)

    const multi_backpack_info* binfo; // 0x00
    override_val<float> m_fuel_use_per_second; // 0x04
    override_val<float> m_fuel_regen_per_second; // 0x10
    override_val<float> m_fuel_activation_cost; // 0x1C
    override_val<float> m_minimum_fuel_to_activate; // 0x28
    override_val<bool> m_player_deactivatable; // 0x34
};

// size 0x34
struct explosion_override_info
{
    void init(); // 0x0095F270 (:80)

    const explosion_info* einfo; // 0x00
    override_val<float> m_knockdown_radius; // 0x04
    override_val<unsigned int> m_human_min_damage_hitpoints; // 0x10
    override_val<unsigned int> m_human_max_damage_hitpoints; // 0x1C
    override_val<float> m_impulse_magnitude; // 0x28
};

// size 0x90
struct weapon_override_info
{
    void init(); // 0x0095F280 (:104)

    const weapon_info* winfo; // 0x00
    override_val<unsigned short> m_magazine_size; // 0x04
    override_val<unsigned short> m_magazine_start_num; // 0x0A
    override_val<unsigned short> m_max_rounds; // 0x10
    override_val<float> m_min_spread; // 0x18
    override_val<float> m_max_spread; // 0x24
    override_val<float> m_fine_aim_min_spread; // 0x30
    override_val<float> m_fine_aim_max_spread; // 0x3C
    override_val<float> m_red_range; // 0x48
    override_val<float> m_headshot_multiplier; // 0x54
    override_val<float> m_min_range_threshold; // 0x60
    override_val<float> m_min_range_player_damage; // 0x6C
    override_val<float> m_max_range_threshold; // 0x78
    override_val<float> m_max_range_player_damage; // 0x84
};

// size 0x8: the backpack overrides of mp_backpack_overrides.xtbl (rfg.pdb; placement inferred)
class backpack_override
{
public:
    backpack_override_info* backpack_infos; // 0x00
    int num_infos; // 0x04
};

// size 0x8: the explosion overrides of mp_explosion_overrides.xtbl
class explosion_override
{
public:
    explosion_override_info* explosion_infos; // 0x00
    int num_infos; // 0x04
};

// size 0x8: the weapon overrides of mp_weapon_overrides.xtbl
class weapon_override
{
public:
    weapon_override_info* weapon_infos; // 0x00
    int num_infos; // 0x04
};

extern backpack_override Backpack_overrides; // 0x02FE80F4
extern explosion_override Explosion_overrides; // 0x02FE6C68
extern weapon_override Weapon_overrides; // 0x02FE9078

void multi_game_mp_overrides_init(mempool_base& pool); // 0x00981FA0 (:451)
void multi_game_overrides_apply_overrides(); // 0x00981FC0 (:458)
void multi_game_overrides_restore_overrides(); // 0x00981FD0 (:465)
