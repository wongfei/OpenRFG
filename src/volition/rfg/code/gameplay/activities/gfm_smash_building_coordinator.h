// Original: volition/rfg/code/gameplay/activities/gfm_smash_building_coordinator.h
// Implementation: volition/rfg/code/gameplay/activities/gfm_smash_building_coordinator.cpp
//
// The smash building coordinator (smc): EDF orders to attack buildings the player is near, at most a few at a time.
#pragma once

#include "volition/vlib/code/util/varray.h"

class gfm_orders;
class object;

// size 0x24
class gfm_smash_building_coordinator
{
public:
    gfm_smash_building_coordinator(); // 0x007AF2B0 (.h:40)
    void level_init(); // 0x007A9C40 (:283)
    bool do_frame(); // 0x007A9CC0 (:299)
    int get_orders_count() const;
    int get_filled_orders_count() const; // 0x007A2F40 (:327)
    void add_new_orders(int index, const farray<object const*,16>& building_list); // 0x007B39B0 (:177)
    bool is_something_else_attacking(unsigned int mover_handle, gfm_orders* ignore_orders); // 0x007A3020 (:346)
    void render_icons(); // 0x0079B040 (:379)
private:
    void destroy_old_orders(); // 0x007A9BB0 (:233)
public:
    ~gfm_smash_building_coordinator(); // 0x00771640

private:
    farray<unsigned int,5> orders_list; // 0x00
};

void smc_level_init_pre(); // 0x007A9D90 (:401)
bool smc_do_frame(); // 0x007A9DA0 (:407)
