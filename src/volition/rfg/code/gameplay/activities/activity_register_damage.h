// Original: volition/rfg/code/gameplay/activities/activity_register_damage.h
// Implementation: volition/rfg/code/gameplay/activities/activity_register_damage.cpp
//
// Damage and deaths reported to activities: objects mark each other on damage, and damage, deaths, building damage and
// explosions are passed to the running activities (and the morale / mayhem counters). The H3 destruction code calls in
// here.
#pragma once


struct explosion_info;
class object;
class rfg_mover;

void register_mark_object(const object* objp); // 0x00770510 (:84)
void activity_register_damage_mark_objects(const object* obj_a, const object* obj_b); // 0x00799C60 (:123)
void activity_register_damage(const object* obj_a, const object* obj_b, float damage, int original_hitpoints); // 0x00799CA0 (:142)
void activity_register_death(const object* target_obj, const object* attacker_obj); // 0x00759DB0 (:160)
void activity_register_building_damage(const rfg_mover* objp, float damage_percent_delta, float original_damage_percent); // 0x00759E00 (:176)
void activity_register_building_destroyed(const rfg_mover* objp); // 0x00759E70 (:192)
void activity_register_explosion(const object* owner, const explosion_info* p_explosion_info); // 0x00759F20 (:220)
bool activity_register_damage_is_player_controlled(const object* objp); // 0x00785790 (:247)
float activity_register_damage_get_cost(const object* objp); // 0x007705C0 (:309)
void activity_register_damage_do_frame(); // 0x00770860 (:430)
void activity_register_damage_level_init(); // 0x007708C0 (:451)
void activity_register_damage_render_debug(); // 0x007858D0 (:472)
