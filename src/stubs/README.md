# src/stubs

Stand-ins (new code, not recovered) for the functions and globals of rfg.exe that `OpenRFG` links and that are not
implemented yet. Each one is removed when its original definition lands in its original file (`stub_impl.md`); the
directory stays for the ones deferred or potentially unused by the game (`deferred.md`). Since S1 every file here is compiled by `OpenRFG`, without `OPENRFG_*`
gates, and nothing here is test code: the test programs link these too, plus their own stand-ins and seams in
`tests/stubs` (same layout, `tests/stubs/stubs.cmake`) and in their folders (`headless_stubs_*.cpp`, ...).

## Contents

Only `deferred.md`'s stand-ins are left: `havok/havok.cpp` and `havok/havok_auto.cpp` (S4), and
`keen/graphics/graphics_framework.cpp` (S2). `stubs.h` stays for `tests/stubs`. The module files that held no entry any
more were deleted on 2026-11-08. `stubs.cmake` globs, so nothing depends on them, and `game_stubgen.py --split` creates
a module's file again if a link ever needs one. Older notes and plans name those files.

## Layout

One folder per original module of the stubbed symbol (its source file in rfg.pdb, else the header that declares it):

| Folder | Original code |
|---|---|
| `rfg/<dir>/` | `volition/rfg/code/<dir>/` (ai, gameplay, interface, multi, objects, vehicle, ...) |
| `rfg/` | the files directly in `volition/rfg/code/` (`rfg.cpp`) |
| `render_lib/`, `vlib/`, `vlogger_lib/` | the volition libraries |
| `havok/` | Havok 2014.2.5 glue: the tagfile reader, the generated no-ops |
| `keen/<module>/` | `library/modules/runtime/<module>/` |
| `game/` | `game/` (keen::rfg::Application, the Steam achievements) |
| `thirdparty/<library>/` | `library/modules/thirdparty/<library>/` and the DLLs rfg.exe imports (sw_api, discord_rpc) |

A folder's files are named by **scope**, the set of programs that link them (`stubs.cmake` sorts them into the
`OPENRFG_STUBS_*` lists by file name; `<stem>` is the folder's name):

| File | List | Linked by |
|---|---|---|
| `<stem>.cpp`, `<stem>_*.cpp` (other) | `OPENRFG_STUBS_COMMON` | every program on the headless stand-ins: the headless tests, `OpenRFG` |
| `<stem>_multi.cpp` | `OPENRFG_STUBS_MULTI` | the program with the real multiplayer core (`OPENRFG_MULTI_CORE`): `OpenRFG` |
| `<stem>_main.cpp`, `<stem>_main_auto.cpp` | `OPENRFG_STUBS_MAIN` | the program on the original boot: `OpenRFG` |
| `<stem>_framework.cpp` | `OPENRFG_STUBS_FRAMEWORK` | the program on the original entry: `OpenRFG` |
| `<stem>_render.cpp` | `OPENRFG_STUBS_RENDER` | the program with rfg_render and the headless stand-ins: `OpenRFG` |

`stubs.h` has the stream2 callback stand-ins' macros (`STUB_PRIM_CALLBACKS`, `STUB_CONTAINER_CALLBACKS`: no counters,
the stand-in trace counts their calls) and `INVALID_HANDLE`. The test half (the counters and the counting
`HEADLESS_*_CALLBACKS`, the fake render objects, the Havok leak tracker, the multiplayer settings of the headless
programs) is `tests/stubs/test_stubs.h`; the headless driver's own interface is
`tests/headless/headless_stubs.h`.

## Generated files

- `*_main_auto.cpp`: `python tools/headless/game_stubgen.py --split src/stubs _main_auto [--abort] <OpenRFG link log>...`
  adds the symbols a link of `OpenRFG` misses to the file of their module and keeps every entry where it is.
- `havok/havok_auto.cpp`: `python tools/headless/havok_stubgen.py <link log>...`.

Write a stand-in with behaviour by hand in the module's file of the right scope; a symbol defined there is never
generated. No new hand-written stand-in is added here after S1 (`stub_impl.md` rule 1): one a port needs is generated.

## Trace and inventory (stub_impl.md S0)

- Every stand-in function starts with `GAME_STUB_HIT("<name>");` (`stub_trace.h`): the generators write it with the
  entry's linker signature, a hand-written stand-in names its qualified name (write it by hand in new ones; the
  callback macros have it). `OpenRFG --stub-trace FILE` writes the stand-ins the program reached;
  `stub_trace_calls(name)` gives a test the calls of a shared stand-in.
- `python tools/headless/stub_inventory.py [--root DIR]... [--trace FILE]... [--tsv OUT] [--files]` lists every entry
  with its scope, gate, the programs that compile it, its rfg.pdb source and its `stub_impl.md` tier (`--root
  tests/stubs` for the test programs' own); `--closure DIR --implemented FILE` writes each tier's
  `tools/ghidra/GameClosure.java` inputs.

## Former files

Until 2026-10-29 the stand-ins were grouped by the test tier that needed them; until 2026-10-30 (S1) the test-only ones
were here behind gates. Older notes and plans name those files:

| Former file | Now |
|---|---|
| `headless_globals.cpp` | the globals of their modules' files (the boot's in `tests/stubs/rfg/rfg_noboot.cpp`) |
| `headless_stubs.cpp` | the common files of `vlib`, `rfg/util`, `rfg/geom`, `rfg/level`, ...; the counters in `tests/stubs/test_stubs.cpp` |
| `headless_stubs_h2.cpp` | `tests/stubs/render_lib/render_lib_noboot.cpp` (the fake render objects), `rfg/pathfinding`, `rfg/spawn`, `rfg/gameplay`, `rfg/level` (save and restore), ... |
| `headless_stubs_h3.cpp` | `tests/stubs/vlib/vlib_noboot.cpp` (the scheduler, os, console), `tests/stubs/rfg/rfg_noboot.cpp`, `rfg/vehicle`, `tests/stubs/rfg/multi/multi_nomulti.cpp` |
| `headless_stubs_h3_seams.cpp` | the files of `rfg/effects`, `rfg/objects`, `rfg/gameplay`, `rfg/multi`, `rfg/event_logging`, `rfg/ai`, ... |
| `headless_stubs_player.cpp` | the files of `rfg/objects`, `rfg/gameplay`, `rfg/interface`, `rfg/ai`, ... |
| `headless_stubs_havok.cpp`, `headless_stubs_havok_auto.cpp`, `headless_havok_tagfile.cpp` | `havok/havok.cpp` (the leak tracker: `tests/stubs/havok/havok_noboot.cpp`), `havok/havok_auto.cpp`, `havok/havok_tagfile.cpp` (deleted in `stub_impl.md` S4 SD: the real serializer) |
| `mp_stubs.cpp`, `mp_stubs.h` | the `*_multi.cpp` files (the multiplayer tests' own were deleted with those tests, 2026-11-04) |
| `main_stubs.cpp` | the `keen/*/*_framework.cpp` files |
| `main_stubs_game.cpp` | the `*_main.cpp` files |
| `main_stubs_auto.cpp` | the `*_main_auto.cpp` files |
| `render_game_seams.cpp` | the `*_render.cpp` files (`headless_pre_stream_render_tasks`: `tests/stubs/rfg/gamewide/gamewide_render.cpp`) |
| `headless_stubs.h` | `stubs.h`, `tests/stubs/test_stubs.h` (the test stand-ins' part) and `tests/headless/headless_stubs.h` (the driver's part) |
