// Original build: the animation half of rfg/code/geom is one translation unit (name of this unity file inferred;
// player_minimal.md P2).
//
// vanim.cpp reads the file statics of anim.cpp (Anims, Num_anims): their bss lies interleaved with the vanim and
// character globals in rfg.exe. The sources are included in path order; only the implemented ones are listed.

#include "volition/rfg/code/geom/anim.cpp"
#include "volition/rfg/code/geom/character.cpp"
#include "volition/rfg/code/geom/character_instance.cpp"
#include "volition/rfg/code/geom/character_mesh.cpp"
#include "volition/rfg/code/geom/character_mesh_shared.cpp"
#include "volition/rfg/code/geom/cloth_simulation.cpp"
#include "volition/rfg/code/geom/cloth_simulation_shared.cpp"
#include "volition/rfg/code/geom/cloth_simulation_shared_spu.cpp"
#include "volition/rfg/code/geom/game_rig.cpp"
#include "volition/rfg/code/geom/mesh_deform.cpp"
#include "volition/rfg/code/geom/mesh_morph.cpp"
#include "volition/rfg/code/geom/vanim.cpp"
