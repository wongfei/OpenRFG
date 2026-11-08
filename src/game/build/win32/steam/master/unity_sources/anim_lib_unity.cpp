// Original build: anim_lib is one translation unit (name of this unity file inferred; player_minimal.md P2).
//
// The anim_lib sources share file statics and file-static functions (the page pool lists and entity bone infos of
// anim_lib_base.cpp, the compressed key readers of anim_lib_animation.cpp, the instance table and bone data readers of
// anim_lib_instance_manager.cpp), so they are compiled together, in the order their declarations need.
// anim_lib_spinebend.cpp (P4) reads the instance table and comes last.

#include "volition/anim_lib/code/anim_lib_base.cpp"
#include "volition/anim_lib/code/anim_lib_animation.cpp"
#include "volition/anim_lib/code/anim_lib_instance_manager.cpp"
#include "volition/anim_lib/code/anim_lib_instance.cpp"
#include "volition/anim_lib/code/anim_lib.cpp"
#include "volition/anim_lib/code/anim_lib_spinebend.cpp"
