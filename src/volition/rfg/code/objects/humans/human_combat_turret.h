// Original: volition/rfg/code/objects/humans/human_combat_turret.h
// Implementation: volition/rfg/code/objects/humans/human_combat_turret.cpp
//
// A human's use of turrets: reserving, manning and abandoning one, and the per-frame update of the operator.
#pragma once


class human;
class turret;

turret* human_combat_get_reserved_turret(const human* hp); // 0x00A55640 (:42)
turret* human_combat_get_manned_turret(const human* hp); // 0x00A556F0 (:64)
bool human_combat_can_man_turret(const human* hp, const turret* tp); // 0x00A55750 (:89)
human* human_combat_turret_get_operator(const turret* tp); // 0x00A2FB00 (:112)
human* human_combat_turret_get_reserving_human(const turret* tp); // 0x00A557F0 (:157)
bool human_combat_reserve_turret(human* hp, turret* tp); // 0x00AC8570 (:188)
bool human_combat_man_turret(human* hp, turret* tp); // 0x00AB6220 (:213)
void human_combat_abandon_turret(human* hp); // 0x00AC0B20 (:346)
void human_combat_turret_do_frame_pre(turret* tp); // 0x00A55880 (:474)
void human_combat_turret_do_frame_post(turret* tp); // 0x00A2FB30 (:516)

// 0x005DCA60 (.h:28)
inline bool human_combat_is_reserving_turret(const human* hp)
{
    return human_combat_get_reserved_turret(hp) != nullptr;
}

// 0x005DCA80 (.h:29)
inline bool human_combat_is_manning_turret(const human* hp)
{
    return human_combat_get_manned_turret(hp) != nullptr;
}
