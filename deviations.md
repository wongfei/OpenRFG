# Port deviations from rfg.exe

Where the port deliberately does something different from `rfg.exe`, and why. Entries come from the docs of this project and were checked against the current code; they were not re-compared with the binary. Reasons marked *Inferred* are not stated in a doc or comment. Original game bugs that the port keeps are in [original_bugs.md](original_bugs.md). Paths are relative to the repository, `rfg.exe` addresses are virtual addresses.

| Result | Entries |
|---|---|
| In the code, as the doc says | 238 |
| In the code, but different from the doc | 7 |
| Listed in a doc, no longer in the code | 47 |
| Could not be settled | 6 |
| Comment in the code only, not listed in a doc, not checked individually | 258 |

The counts above are from the original check. A later comparison with the binary removed 24 rows and added 4, and line numbers of other rows in the edited files may be off by a few lines.

## Deviations by module

### Havok SDK

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/uuid/hkuuid.cpp:4` | `hkUuid` | - | hkUuid has no setFromString, isValidUuidString, toString, combinations or hkUuidObject | No caller in the port |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/compat/patches/660/hkpatches_660.cpp:129` | `hkxVertexBuffer_0_to_1` | `0x00F9C930` | The two identical blend weight/index blocks of hkxVertexBuffer_0_to_1 are one case in the port. | Inferred: the port merges identical switch cases. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/internal/geometryprocessing/convexhull/hkgpconvexhullinternals.cpp:226` | `hkgpConvexHullImpl::build` | `0x00FF9B40` | The convex hull build sets a pending vertex's m_refs to 0, where rfg.exe copies an uninitialized stack word. | Every use of m_refs resets it first, so the value is irrelevant and 0 is deterministic. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/classes/reflections/hkrootlevelcontainerreflection.cpp:4` | - | - | Generated Havok reflection data uses the image's member offsets and object sizes, not HK_OFFSET_OF and sizeof. *Same header in every generated reflection file.* | Generated from rfg.exe by reflgen.py, so the numbers are the image's. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/classes/reflections/hkrootlevelcontainerreflection.cpp:8` | - | - | Static Members arrays of classes whose src header does not spell them out, or that are not in src, are file statics, listed at the top of each reflection file. *Many reflection files carry this comment.* | reflgen.py finds no Members[] to define them on. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/classes/reflections/hkrootlevelcontainerreflection.cpp:11` | - | - | Type infos (finish/cleanup/getVtable, <Class>TypeInfo) are not generated for classes not declared in src. *Many reflection files carry this comment.* | They need every class's finishing constructor and a class declaration in src. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.cpp:397` | - | - | hkRefPtr members are released in destructor bodies, because hkRefPtr has no destructor in this port. | hkRefPtr has no destructor in this port. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/native/hkdataobjectnative.cpp:782` | - | - | The registry references and parser temporaries are released explicitly since hkArray does not destroy elements and hkRefPtr has no destructor. *Only this site was checked, not StartElement or hkTree::Node.* | hkRefPtr has no destructor in this port. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/xml/hkxmlobjectreader.cpp:1055` | `hkXmlObjectReader::base64read` | `0x01054C60` | base64read masks the character to 7 bits before the table lookup, where rfg.exe reads the table with the unmasked value. | Avoids reading past the table for bytes >= 0x80. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/binary/hkbinarytagfilereader.cpp:112` | - | `0x00F6C140` | The binary tagfile reader's ForwardReferences::remember copy-constructs the value handles into the array as the SDK's pushBack does. | The port's pushBack assigns and the handles' operator= writes through into the world. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/tagfile/xml/hkxmltagfilereader.cpp:936` | - | - | Warnings rfg.exe compiles in are written out with hkErrStream and hkErrorFwd::messageWarning at their ids and lines. *Same calls in hkversionpatchmanager.cpp, hkbinarytagfilereader.cpp, hkpackfilewriter.cpp.* | Inferred: the warning macros are not available, so the calls are spelled out. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkbuiltintyperegistry.cxx:8` | - | - | The built-in type registry's StaticLinkedTypeInfos leaves out type infos of classes not declared in src or whose finishing constructor is not ported. *Counts 130 + 10 not rechecked.* | Those classes' type infos need declarations or finish constructors not yet ported (list in UNPORTED_FINISH_CLASSES of reflgen.py). |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/xml/hkfloatparseutil.cpp:37` | `loadThroughX87` | - | hkFloatParseUtil moves its NaN table values through loadThroughX87, which quiets the two signalling NaNs as rfg.exe's x87 loads do. *Matches rfg.exe, so arguably not a deviation; the doc lists it as one.* | Reproduces rfg.exe's x87 quieting of signalling NaNs. |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/dynamics/inertia/hkpinertiatensorcomputer.cpp:54` | `scaleVolumeMassProperties` | `0x00F1D880` | scaleVolumeMassProperties writes 0 to the w lanes of the new inertia columns, which rfg.exe leaves as stack garbage. | Inferred: deterministic output instead of uninitialised stack data. |

### audio_lib

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/audio_lib/code/audiolib.cpp:1569` | `audiolib_cue_get_id` | `0x0048DC50` | audiolib_cue_get_id(name) calls a null-by-default log hook with the cue name and id after a successful lookup, so the --audio-log test seam can learn names. *The hook is set by src/openrfg/main_audio_log.cpp (OpenRFG --audio-log).* | The retail data has no cue names, so the log learns them from the lookups by name. |
| `src/volition/audio_lib/code/audiolib.cpp:1674` | `audiolib_cue_play` | - | audiolib_cue_play is a wrapper around rfg.exe's body (audiolib_cue_play_internal) that logs each result through a null-by-default hook when the --audio-log seam is on. *No VA comment on the wrapper itself.* | Test seam for --audio-log: logs every cue play result without changing the game's behaviour. |

### game

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/game/sources/application.cpp:437` | - | - | --no-steam clears g_openrfgUseSteam; then no Steam save data system or system services are created. *Comment at application.hpp:109 and application.cpp:608.* | Lets the port run without Steam on request; rfg.exe always uses Steam. |
| `src/game/sources/application.cpp:612` | `Application::fillGameframeworkParameters` | `0x00459D80` | fillGameframeworkParameters sets useSteam from the --no-steam seam (g_openrfgUseSteam), where rfg.exe sets it true and exits when Steam does not start. Also: Application::fillGameframeworkParameters sets useSteam from the --no-steam seam g_openrfgUseSteam (true by default), where rfg.exe sets it true and exits if Steam does not start. | The port can run without Steam on request (orig_main.md 2.5, stub_impl.md S2). |
| `src/game/sources/application.cpp:627` | `Application::fillGameframeworkParameters` | `0x00459D80` | --window-pos X Y places the window frame at physical desktop pixels instead of rfg.exe's fixed (60, 60) client area, converting for the DPI-unaware process. | Lets multiple instances be placed on screen. |
| `src/game/sources/application.cpp:670` | - | - | With --no-steam, --no-steam-cloud-saves or --save-dir, save data goes to the local SaveDataProviderWin32Native (one keen_savegame file in %APPDATA%/OpenRFG/save or --save-dir) instead of rfg.exe's Steam remote storage. By default it uses Steam remote storage, as rfg.exe always does. | Lets a run keep away from the user's real Steam Cloud saves, so that a port bug cannot damage them. |
| `src/game/sources/application.cpp:735` | `Application::initialize` | `0x00459F30` | Application::initialize mounts /gamebuild from the -gamebuild DIR option when given, instead of always <exe dir>/data. Also: -gamebuild DIR on the command line mounts DIR as /gamebuild instead of <exe dir>/data; off by default. | The program can run outside the game folder, for the tests' data directory. |
| `src/openrfg/CMakeLists.txt:21` | - | - | OpenRFG.exe embeds the MSVC linker's default manifest (`requestedExecutionLevel` asInvoker, nothing else); rfg.exe has no manifest at all. | Kept on purpose (user decision 2026-11-09): without a manifest Windows treats the program as a legacy app and silently redirects writes to protected folders to VirtualStore. |

### keen/base

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/library/modules/runtime/base/sources/win32/nativefile_win32.cpp:328` | - | - | A forced shell32 import (pragma lib and /include of __imp__SHGetSpecialFolderPathW) next to getKnownWindowsFolder so SHGetKnownFolderPath can be found via GetModuleHandleW. | rfg.exe imports shell32 itself; the port had no import, so the options ini was never read. |

### keen/gameframework

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/library/modules/runtime/gameframework/sources/global/gameframework.cpp:78` | `loadFrameworkMessage` | - | loadFrameworkMessage empties the buffer before LoadStringW, so a failed lookup logs nothing. *The message boxes themselves are now replaced by log lines (logFrameworkMessage).* | rfg.exe leaves its uninitialized stack buffer as it was when LoadStringW finds nothing. |
| `src/library/modules/runtime/gameframework/sources/global/gameframework.cpp:81` | `loadFrameworkMessage` | - | The framework's message boxes are replaced by log lines, with their text from the string table (gameframeworklocatexts.rc). *logFrameworkMessage prints a description alongside; gameframework_win32.cpp has the same change for crash dialogs.* | Made while the string table was not linked and the boxes would have been empty; it is linked now, so the boxes could be restored. |
| `src/library/modules/runtime/gameframework/sources/global/gameframework.cpp:543` | `GameFramework::initialize` | `0x00C10190` | GameFramework::initialize calls network::initializePlatform (WSAStartup) where the system services are created; rfg.exe starts winsock only through Steam. Also: GameFramework::initialize starts winsock through network::initializePlatform, which rfg.exe leaves to curl's global init or Steam. *Doc says the system services stub starts winsock; it is now in the real GameFramework::initialize.* | Nothing on rfg.exe's LAN path starts winsock without Steam, and without it the LAN socket and address lookup fail. |
| `src/library/modules/runtime/gameframework/sources/global/gameframework.cpp:547` | `GameFramework::initialize` | `0x00C10190` | The winsock start runs ungated inside GameFramework::initialize (network::initializePlatform), not in systemservices_framework.cpp. Also: WSAStartup is run through network::initializePlatform when the system services are created. *Now in gameframework.cpp, not systemservices_framework.cpp, with no #ifdef gate.* | Ungated so every test program and OpenRFG get sockets; same reason as the orig_main.md 2.5 deviation. |
| `src/library/modules/runtime/gameframework/sources/global/gameframework.cpp:548` | `GameFramework::initialize` | `0x00C10190` | When Steam is off, GameFramework::initialize sets pSystemServices to nullptr; rfg.exe leaves it unwritten. Also: With --no-steam the framework creates no system services system (pSystemServices set to null); rfg.exe leaves it unwritten when no parameters. | The framework system's constructor initializes nothing, so the field held garbage and crashed the first frame. |
| `src/library/modules/runtime/gameframework/sources/win32/backtraceuploader.cpp:27` | `keen::uploadToBacktrace` | `0x00C0E870` | uploadToBacktrace does nothing: rfg.exe posts the minidump and log to the Backtrace URL with libcurl, the port sends nothing. Also: uploadToBacktrace does nothing: the crash dump and log are never sent to the Backtrace endpoint. | A reimplementation must not upload crash reports to the publisher's endpoint (orig_main.md 3.4). |
| `src/library/modules/runtime/gameframework/sources/win32/gameframework_win32.cpp:308` | `handleFatalError` | `0x00C0FF20` | handleFatalError shows no crash question box: the crash is logged and the dump is always written as if the user answered yes. | Message boxes are replaced by log lines (made while the string table was not linked). |

### keen/gamelink

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp:35` | - | - | A global local LAN port (0 = original) lets several instances on one machine bind, advertise and see each other in GameLinkLan; rfg.exe binds only the matchmaking port and ignores packets from its own IP. *Now set by OpenRFG automatically (main_ui_command_line.cpp), not by a --lan-port option; --lan-port no longer exists in tests.* | Two instances on one machine cannot see each other as shipped, and tests and OpenRFG need that. |
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp:40` | - | - | Off by default, kaiko::gamelink::g_openrfgLanLocalPort binds the LAN socket to a chosen port and compares IP and port in own-address checks and the LAN uid, so several instances share one machine. Also: kaiko::gamelink::g_openrfgLanLocalPort binds the LAN link to a chosen port and compares IP and port in own-address checks (--lan-port). *Also used in multi_core_platform_generic.cpp:645 (port in the LAN uid).* | Several instances on one machine must see each other; the original compares own address by IP only and binds the matchmaking port (multiplayer.md 2.3). |
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp:62` | `openrfgGetLanOwnHostAddress` | - | With the local-port option on, the host's own LAN address is the source address of the default route (a probe UDP socket connected to TEST-NET), not the first local host address. Also: With a local LAN port set, the own host address is the default route's source address (UDP socket connected to a TEST-NET address) instead of the first address of getHostNetworkAddress. *Not mentioned in the README row; added after it.* | On machines with several adapters the first address need not be the one peers see, so clients dropped the lobby. |
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp:243` | `GameLinkLan::beginSession` | `0x00CD3360` | GameLinkLan::beginSession binds its socket to the local LAN port when set, instead of always the matchmaking port. | Several instances on one machine need distinct ports. |
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp:610` | `GameLinkLan::createLobby` | `0x00CD6FA0` | The host's own station (first lobby member) uses the own host address on the local LAN port rather than the matchmaking port. *The same replacement is made for the client's own station in handlePacket's lobby state update.* | The own-station check must match the port the instance actually uses. |
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp:1012` | `GameLinkLan::handlePacket` | `0x00CD71B0` | The lobby-packet self filter compares IP and port (isOwnAddress) instead of the IP only, so other instances on the same IP are not ignored. | Instances sharing one IP must see each other's lobby packets. |
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp:1210` | `GameLinkLan::isLobbyInfoRelevant` | `0x00CD39F0` | isLobbyInfoRelevant dedupes search results by IP and port when the local port is set; rfg.exe dedupes by IP only. | Several hosts on one machine are separate lobbies. |
| `src/library/modules/runtime/gamelink/sources/steam/gamelinkinternet_steam.cpp:213` | `GameLinkInternet::beginSession` | - | GameLinkInternet::beginSession fails without Steam, so the internet link stays offline. Also: GameLinkInternet::beginSession fails when SteamUser() is null. | Keeps the internet link offline so LAN play works as before. |

### keen/gamesession

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/library/modules/runtime/gamesession/sources/global/localgamesession.cpp:590` | - | - | LocalGameSession without a user account system goes on to the save data as an anonymous user, with hasExpectedUserId false | Without Steam there is no user account system, but local saves must still work. |

### keen/savedata

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/library/modules/runtime/savedata/sources/win32/savedataprovider_win32_native.cpp:41` | `SaveDataProviderWin32Native::create` | - | Saves go to the --save-dir folder, else roaming app data OpenRFG/save, by a native win32 save data provider. Also: The save location is OpenRFG's own folder (or --save-dir), not rfg.exe's save path. | Inferred: no Steam cloud or retail save location available in the port. |
| `src/library/modules/runtime/savedata/sources/win32/savedataprovider_win32_native.cpp:362` | `SaveDataProviderWin32Native::writeSaveData` | - | The native save provider writes through a temporary file named after the process and swaps it in, keeping the previous file as .bak Also: Saves are written to a temporary file and a .bak copy of the previous file is kept. *The comment above the function describes the temp-file swap; the .bak copy is mentioned only at the erase path.* | Inferred: a crash during a write must never leave a torn save file. |

### keen/systemservices

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/library/modules/runtime/systemservices/sources/steam/systemservices_steam.cpp:139` | - | - | Steam-backed functions return early with a 'deviation: no Steam' guard when the system services object is null, answering as the old stand-in did. Also: Steam-backed functions carry '// deviation: no Steam' guards that return early when Steam is absent. *Same guard in systemservices.cpp, useraccountsystem_steam.cpp and savedatahandler.cpp.* | With --no-steam there is no Steam system, so these functions answer for a null system. |

### render_lib

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/render_lib/code/rl_deformable_mesh_instance.cpp:133` | `rl_deformable_mesh_instance::on_visible_for_frame` | - | The frame's dent weights are uploaded from the visibility pass on the immediate context (as rl_skin_instance does for face morphs), not from the instance's first draw. *Not run in game; the D3D11 refusal was measured with a scratch probe on the Intel, NVIDIA and WARP adapters.* | In rfg.exe the first draw races between part 1's depth pre-pass (immediate context) and part 2's base pass (deferred context). A part 2 win at a nonzero buffer offset maps WRITE_NO_OVERWRITE on a deferred context without a prior discard, D3D11 returns DXGI_ERROR_INVALID_CALL, and the vehicle draws undented against a possibly dented pre-pass: flicker on damaged vehicles. |
| `src/volition/render_lib/code/rl_particle_renderer.cpp:537` | `rl_particle_renderer::draw_particles` | `0x004C3F40` | draw_particles makes the D3D11 calls itself (unmap immediate buffer, bind it with the shared corner-uv stream and the type's index buffer, DrawIndexed) instead of a keen command. Also: draw_particles makes raw D3D11 calls for the immediate geometry with a second stream and an index buffer. | rfg.exe inlines keen's immediate geometry end with a second stream and an index buffer, which keen has no command for. |
| `src/volition/render_lib/code/rl_particle_renderer.cpp:538` | `rl_particle_renderer::draw_particles` | `0x004C3F40` | rl_particle_renderer::render and render_corona_internal draw through draw_particles as raw D3D11 (second stream m_uv corner uvs, index buffer). *The comment is at the function header.* | keen's immediate geometry has no second stream or index buffer command. |
| `src/volition/render_lib/code/rl_renderer.h:80` | - | - | rl_renderer befriends rl_layered_material. | Inferred: the layered material reaches renderer internals that rfg.exe's unity build exposes. |
| `src/volition/render_lib/code/rl_shader_fixup.cpp:13` | - | - | A load-time patch rewrites the bloodsplat decal shaders' DXBC to fix an undecoded normal read. *Reason taken from the file header ('load-time fixes of shader data bugs') and the blood-decal audit.* | Fixes a shader data bug; shipped bytecode misbehaves on this hardware. |
| `src/volition/render_lib/code/rl_shader_fixup.cpp:279` | `fixup_skybox_dome` | - | A load-time patch clamps the skybox dome shader's DXBC at the apex. | Fixes a NaN (asin of sqrt) at the dome top that left a black circle. |
| `src/volition/render_lib/code/rl_shader_fixup.cpp:367` | `rl_shader_fixup_vertex_shader` | - | The sky dome vertex shader's clamp immediate 10.0 is patched to 9.99999905 at load time (fixup_skybox_dome). *Comment in rl_shader_fixup.h.* | A fused mad gives NaN at the dome apex, a black circle looking straight up. |
| `src/volition/render_lib/code/rl_shader_fixup.cpp:370` | `rl_shader_fixup_vertex_shader` | - | Decal vertex shaders rfg-decal-bloodsplat_c, _mc, _ms and _t are patched at load time to decode the vertex normal. Also: Load-time DXBC patch of the bloodsplat vertex shaders (normal decode). Also: Load-time DXBC patch of the bloodsplat vertex shaders in rl_shader_fixup.cpp. *The comment is in rl_shader_fixup.h.* | The rfg.exe shader bug (undecoded UNORM normal) hid blood decals for some hit directions. |
| `src/volition/render_lib/code/rl_shader_fixup.h:4` | - | - | A load-time DXBC fix-up patches the bloodsplat_c/_mc/_ms/_t decal vertex shaders to decode the UNORM vertex normal, which rfg.exe's shipped shaders read undecoded. Also: Decal vertex shaders rfg-decal-bloodsplat_c, _mc, _ms and _t are patched at load time to decode the vertex normal (DXBC rewrite, checksum rebuilt). *Shader data patch, not rfg.exe code; implementation in rl_shader_fixup.cpp. Also bug_blood_decals.md:6.* | Without the decode, decals on surfaces facing -x/-y/-z get alpha 0, so blood marks show only for some hit directions. |

### rfg

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/rfg/code/rfg.cpp:1773` | `allow_remote_session` | `0x005D9660` | -allowremote on the command line skips rfg_pre_initialize_game's remote desktop refusal; off by default. *VA comment belongs to rfg_pre_initialize_game; the helper is new.* | Allows development over RDP. |

### rfg/ai

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/rfg/code/ai/ai_script_actions.cpp:26` | - | - | Every file defining AI goals or actions has a numbered #pragma init_seg(".CRT$XCUAG<n>") following its position in rfg.exe's table. *Many ai files carry one.* | goal_pool/action_pool order decides planner ties and the link order differs from rfg.exe's .CRT$XC table. |
| `src/volition/rfg/code/ai/ambient_action_manager.cpp:528` | `collect_vehicle_npcs` | - | collect_vehicle_npcs is a new static helper for the aboard-npc collection that rfg.exe inlines in the drop-off and park actions. | Gathers a repeated inlined block into one function. |
| `src/volition/rfg/code/ai/civilian/civilian_restricted_area_actions.cpp:278` | `find_threatening_npc` | - | find_threatening_npc is a new static helper for the search that rfg.exe inlines in both civilian restricted area get_cost functions. | Gathers a repeated inlined block into one function. |
| `src/volition/rfg/code/ai/civilian/pedestrian_actions.cpp:318` | `finish_ambient_vehicle_action` | - | finish_ambient_vehicle_action, waiting_for_passengers and ambient_drive_cost are new static helpers for blocks rfg.exe inlines in the ambient pedestrian vehicle actions. *ambient_drive_cost carries its own comment at the same file.* | Gathers repeated inlined blocks into functions. |
| `src/volition/rfg/code/ai/percept.cpp:4316` | `ai_perception::enter_idle_attention` | - | ai_perception::enter_idle_attention and enter_search_attention are new helpers holding the attention resets that rfg.exe inlines in several functions. *enter_search_attention at percept.cpp is the same kind of helper; declared in percept.h with a 'not in rfg.exe' comment.* | The port gathers the repeated inlined attention reset blocks into one function each. |
| `src/volition/rfg/code/ai/threaten_actions.cpp:83` | `get_desired_civilian` | - | get_desired_civilian and release_civilian_from_order are new static helpers replacing blocks rfg.exe inlines in each oppression action. | Gathers the repeated inlined target check into one function. |

### rfg/camera

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/rfg/code/camera/camera_free.cpp:297` | `unused_dcf_cam_fov` | `0x006CAEC0` | unused_dcf_cam_fov calls camera_free_set_fov and camera_slew_set_fov where rfg.exe inlines them, which reorders the slew fov write and the fov loop. | Inferred: the helpers are used in place of the inlined code. |
| `src/volition/rfg/code/camera/camera_free.cpp:1493` | - | - | In the gyro path the pitch input is read before the heading is applied, because camera_free_apply_offset takes both; the asm reads it between them. *Enclosing function not identified.* | camera_free_apply_offset takes heading and pitch together. |
| `src/volition/rfg/code/camera/camera_free.cpp:1770` | `camera_free_toggle_close` | `0x006CB490` | camera_free_toggle_close drops the debug print for an unknown submode. | Debug-only print. |

### rfg/code_effects

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/rfg/code/code_effects/material_effects_shared.cpp:22` | - | `0x01DEB6F0` | Effect_infos and Num_effect_infos are external although they are file statics in rfg.exe. *Mat_platform_ext is also declared extern in material_effects_shared.h.* | Two TUs of one rfg.exe unity build share them (material_effects.cpp and material_preload_shared.cpp). |
| `src/volition/rfg/code/code_effects/tracers.cpp:361` | `expanding_tracer_particle_spread` | `0x006E9AA0` | expanding_tracer_particle_spread has external linkage (a file static in rfg.exe). | So that the vfxlib classes can befriend it. |

### rfg/cutscene

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/rfg/code/cutscene/cutscene.cpp:80` | - | `0x01E2B444` | The cutscene files are separate translation units where rfg.exe compiles them as one, so Current_cutscene is external rather than a file static | Inferred: the port keeps one source file per original file and shares the global through cutscene_internal.h. |
| `src/volition/rfg/code/cutscene/cutscene.cpp:355` | - | - | The unused cs_camera_index parameters and the name of cs_play_animation_on_character and rack_focus_repair are left out, as LTCG dropped them | LTCG removed the parameters in rfg.exe. |
| `src/volition/rfg/code/cutscene/cutscene.cpp:361` | - | - | tweener gains get_start_time and get_end_time accessors for field reads | Inferred: the cutscene code reads the tweener's key range, which rfg.exe reads from the fields. |
| `src/volition/rfg/code/cutscene/cutscene.cpp:549` | - | - | cs_audio_prepare_blocking, cutscene_hide_weapons, str_new_copy, cs_parse_navpoint and similar are separate functions called where rfg.exe inlines them *Several sites carry '// rfg.exe inlines this call.'; the anchor is the first.* | Inferred: shares the bodies between callers. |
| `src/volition/rfg/code/cutscene/cutscene.cpp:1298` | `cutscene_start_next_shot` | - | cutscene_start_next_shot skips the shot%u string rfg.exe prints into a local buffer and never reads | The string is never read. |
| `src/volition/rfg/code/cutscene/cutscene_load.cpp:46` | `cs_errorf` | `0x00705FC0` | Errorf is the file-local cs_errorf, empty as in rfg.exe's retail build | File-local so that it cannot meet another Errorf at link time. |
| `src/volition/rfg/code/cutscene/cutscene_load.cpp:770` | - | - | Cutscene load messages print to stack buffers where rfg.exe uses its thread-local one *Also in cutscene_options.cpp (two sites).* | Inferred: the thread-local print buffer is not ported. |

### rfg/effects

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/rfg/code/effects/effects.cpp:53` | - | - | effects.cpp is a split TU: rfg builds the table part, rfg_effects rebuilds it with OPENRFG_EFFECTS for the playback part. | Programs without the effect library still need the tables and state, which the rest of rfg reads. |
| `src/volition/rfg/code/effects/effects.cpp:60` | - | `0x02182CEC` | Effects_preload_srid (and the singleplayer marks) lose their static because both parts of the split effects.cpp use them. *The second part declares it extern at line 361.* | Both halves of the split TU need the same variable. |

### rfg/gameplay

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/rfg/code/gameplay/activities/activity.cpp:2427` | - | - | activity_register_damage_do_frame, _level_init and _mark_objects, gsm_release_alert_level_cap, find_activity_data_internal and similar are called as separate functions where rfg.exe inlines them | Inferred: keeps the callers readable and shares the body. |
| `src/volition/rfg/code/gameplay/activities/activity_area_defense.cpp:119` | `unused_dcf_area_defense_start` | `0x007A89B0` | unused_dcf_area_defense_start leaves out the convoy end point case of the position lookup rfg.exe inlines | An area defense node is never a convoy end point. |
| `src/volition/rfg/code/gameplay/activities/activity_area_defense.cpp:451` | - | `0x00769F20` | get_voice_line_handles takes a normal argument where LTCG used a register; the caller passes null for rfg.exe's cleared EDI | The port cannot express rfg.exe's LTCG register calling convention. |
| `src/volition/rfg/code/gameplay/activities/activity_assassin.cpp:220` | `activity_assassin_data::render_icons` | `0x0076A170` | activity_assassin_data::render_icons skips rfg.exe's read of each bureaucrat's bounding box, whose result is discarded | The read has no effect. |
| `src/volition/rfg/code/gameplay/activities/activity_continuous_attack.cpp:345` | - | - | reset and activity_do_frame_active call release_and_reset, which rfg.exe inlines with the same body | Inferred: shares the body instead of duplicating it. |
| `src/volition/rfg/code/gameplay/activities/activity_continuous_attack.cpp:752` | `activity_cont_attack_end` | - | activity_cont_attack_end calls activity_kill (and start does too), which rfg.exe inlines with the same body | Inferred: shares the body instead of duplicating it. |
| `src/volition/rfg/code/gameplay/activities/activity_convoy.cpp:431` | `activity_convoy_data::process_target_driver_flags` | - | process_target_driver_flags starts from a zeroed flag word; rfg.exe ORs two flags into an uninitialized one, so convoy_vehicle_exit was whatever the stack held | Avoids reading an uninitialized stack word. |
| `src/volition/rfg/code/gameplay/activities/activity_convoy.cpp:969` | `activity_convoy_data::collect_spawn_objects` | - | activity_convoy_data::collect_spawn_objects and render_icons write fields through const_cast because the PDB declares the methods const | rfg.exe writes the fields from const methods anyway. |
| `src/volition/rfg/code/gameplay/activities/activity_convoy.cpp:1803` | `activity_convoy_vehicle_destroy` | - | activity_convoy_vehicle_destroy asks activity_get_mode() where rfg.exe reads activity.cpp's static | The static is private to activity.cpp. |
| `src/volition/rfg/code/gameplay/activities/activity_courier_chase.cpp:535` | - | - | Courier and convoy code calls play_radio_line, request_ai_replan, the base activity_end and activity_kill and similar as functions where rfg.exe inlines them | Inferred: shares the bodies between callers. |
| `src/volition/rfg/code/gameplay/activities/activity_courier_chase.cpp:548` | `activity_courier_chase_data::activity_kill` | `0x007A8DF0` | The courier's activity_kill and activity_end_common check for null orders; rfg.exe reads through null for a handle beyond the pool *The null check is inside the destroy_orders(gfm_orders*) overload. Anchor is the function header because the Deviation comment line occurs twice (also in activity_end_common).* | Avoids a null read for an out-of-pool orders handle. |
| `src/volition/rfg/code/gameplay/activities/activity_courier_chase.cpp:785` | `activity_courier_chase_data::render_hud` | `0x0076BEC0` | The courier's render_hud prints its debug text to a stack buffer where rfg.exe uses its thread-local one | Inferred: the thread-local print buffer is not ported. |
| `src/volition/rfg/code/gameplay/activities/activity_courier_chase.cpp:1125` | `activity_cc_is_active_courier` | `0x00757EB0` | activity_cc_is_active_courier, activity_convoy_vehicle_destroy and activity_delivery_start_activity ask activity_get_mode() where rfg.exe reads activity.cpp's static *The same getter is used in activity_convoy.cpp:1802 and activity_delivery.cpp:912.* | The static is private to activity.cpp. |
| `src/volition/rfg/code/gameplay/activities/activity_delivery.cpp:913` | `activity_delivery_start_activity` | - | activity_delivery_start_activity asks activity_get_mode() where rfg.exe reads activity.cpp's static | The static is private to activity.cpp. |
| `src/volition/rfg/code/gameplay/activities/activity_demolitions_master.cpp:871` | `activity_demolitions_master::activity_do_frame_clean_up` | `0x00797D40` | activity_do_frame_clean_up uses activity_shutdown_period_override(0) where rfg.exe sets the private timer directly | The shutdown timer is private to the base class. |
| `src/volition/rfg/code/gameplay/activities/activity_demolitions_master.cpp:1401` | `activity_demolitions_master::render_icons` | `0x0076D0E0` | The demolitions master render_icons and start ask activity_get_mode() where rfg.exe reads the static Current_activity_mode | The static is private to activity.cpp. |
| `src/volition/rfg/code/gameplay/activities/activity_demolitions_master.cpp:1459` | `activity_demolitions_master_alert_allows_start` | `0x0076D200` | activity_demolitions_master_alert_allows_start reads gsm_get_alert_level() where rfg.exe reads the static Current_alert_level | The static is private; the getter clamps to green and red, which answers the same. |
| `src/volition/rfg/code/gameplay/activities/activity_house_arrest.cpp:175` | `draw_weapon_no_anim` | `0x00758960` | draw_weapon_no_anim and is_roadblock_child take normal arguments where LTCG used registers | The port cannot express rfg.exe's LTCG register calling convention. |
| `src/volition/rfg/code/gameplay/activities/activity_house_arrest.cpp:398` | - | - | activity_end, activity_kill, level_init, record_loss and activity_house_arrest_exit call the base methods rfg.exe inlines, since the activity_end_common pair is private | The private activity_end_common pair cannot be called from the derived class. |
| `src/volition/rfg/code/gameplay/activities/activity_house_arrest.cpp:599` | - | - | get_informant_npc, get_activity_start_node_from_handle, spawn_squad, release_and_reset_squad and others are called as functions where rfg.exe inlines them | Inferred: shares the bodies between callers. |
| `src/volition/rfg/code/gameplay/activities/activity_house_arrest.cpp:878` | - | - | do_frame_start_up reads gsm_get_alert_level() where rfg.exe reads Current_alert_level *The line sits under the comment '0x007B2350 (:1214). rfg.exe inlines house_arrest_can_be_started and do_frame_start_up_inactive.'* | The static is private to the alert level manager; the getter's clamp gives the same answer for the < ALERT_LEVEL_ORANGE test. |
| `src/volition/rfg/code/gameplay/activities/activity_mayhem.cpp:272` | `activity_mayhem::render_hud` | `0x0076E940` | The debug text in activity_mayhem::render_hud prints to a stack buffer where rfg.exe uses its thread-local one | Inferred: the thread-local print buffer is not ported. |
| `src/volition/rfg/code/gameplay/activities/activity_mission_vrail.cpp:108` | `activity_mission_vrail_data::can_initialize` | `0x007A1D60` | activity_mission_vrail_data::can_initialize calls the base, which gives the same answer; rfg.exe also calls is_active and was_initialized and ignores them | The extra calls have no effect. |
| `src/volition/rfg/code/gameplay/activities/activity_raid.cpp:1900` | `raid_find_furthest_stale_squad` | - | The orders walks of do_frame_orders_attacker and _defender share one helper instead of rfg.exe's two inlined lists | Inferred: avoids duplicating the walk. |
| `src/volition/rfg/code/gameplay/activities/activity_register_damage.cpp:344` | `activity_register_damage_draw_box` | `0x00759FC0` | activity_register_damage_draw_box draws nothing | Retail strips the debug draw, and nothing sets Activity_register_damage_debug. |
| `src/volition/rfg/code/gameplay/activities/activity_tanks_a_lot.cpp:241` | `activity_tanks_a_lot::render_icons` | `0x0075A3C0` | activity_tanks_a_lot::render_icons is empty; rfg.exe looks up the player's vehicle and discards the result | The lookup has no effect. |
| `src/volition/rfg/code/gameplay/activities/activity_vrail.cpp:109` | `unused_dcf_riding_shotgun_start` | `0x007A9780` | unused_dcf_riding_shotgun_start leaves out the convoy end point case of the position lookup rfg.exe inlines | A riding shotgun node is never a convoy end point. |
| `src/volition/rfg/code/gameplay/activities/activity_vrail.cpp:299` | `load_riding_shotgun_story_table` | - | load_riding_shotgun_story_table constructs its chapters in the raw allocation with placement new | rfg.exe's operator= sets each array up itself; farray::operator= here needs a constructed object. |
| `src/volition/rfg/code/gameplay/activities/activity_vrail.cpp:882` | - | - | register_death returns early for a null target | rfg.exe reads through null for a null target. |
| `src/volition/rfg/code/gameplay/activities/activity_vrail.cpp:1279` | - | - | do_frame_active_wait_jorge checks the patrol for null before reading its root navpoint | rfg.exe reads through a null patrol. |
| `src/volition/rfg/code/gameplay/activities/activity_vrail.cpp:1436` | - | - | The riding shotgun and mission vrail rails check the squad for null after a failed spawn *Same comment in activity_mission_vrail.cpp:272.* | rfg.exe reads the handle through null when the spawn fails. |
| `src/volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator.cpp:111` | `activity_disable_reinforcements` | `0x0075AFD0` | activity_disable_reinforcements asks activity_get_mode() where rfg.exe reads activity.cpp's static Current_activity_mode Also: activity_disable_reinforcements reads the mode through activity_get_mode instead of activity.cpp's static Current_activity_mode. *The doc says revisit in GF4; gameflow.md:589 says it was left as is.* | The static is private to activity.cpp; the getter returns it unchanged. |
| `src/volition/rfg/code/gameplay/activities/vrail_common.cpp:321` | - | - | update_vrail_progress_complete_wait checks the squad's vehicle for null | rfg.exe reads through null when the squad has lost its vehicle. |
| `src/volition/rfg/code/gameplay/activities/vrail_common.cpp:373` | - | - | do_frame starts its handle invalid, where rfg.exe reads an unset stack word if no rail answers *Anchor is the second line of a two-line comment.* | Avoids an uninitialized stack read. |
| `src/volition/rfg/code/gameplay/activities/vrail_system.cpp:154` | `vrail_get_squads` | `0x0079BAB0` | vrail_get_squads and vrail_stop_squad_spawns ask activity_get_mode() where rfg.exe reads activity.cpp's static | The static is private to activity.cpp. |
| `src/volition/rfg/code/gameplay/air_bombing.cpp:193` | `air_bomb_load_table` | `0x0075C3E0` | air_bomb_load_table calls parse_air_bomb_info as a separate function, which rfg.exe inlines | Inferred: keeps the table loader readable. |
| `src/volition/rfg/code/gameplay/gameflow/alert_level_manager.cpp:1030` | `gsm_do_frame` | `0x007AA800` | gsm_do_frame's alerted-handle removal copies one element fewer than rfg.exe's REP MOVSD | rfg.exe's extra element is read past the array when it is full and is never read again. |
| `src/volition/rfg/code/gameplay/gameflow/alert_level_manager_internal.h:47` | - | `0x01657598` | Alert_level_ranges is external, declared in alert_level_manager_internal.h, for the courier and convoy, where rfg.exe reads it through the unity build | The other files need the table; rfg.exe's unity build shares it. |
| `src/volition/rfg/code/gameplay/gameflow/gfm_control_morale.cpp:186` | `gfm_control_morale_debug_render` | `0x007AFD90` | gfm_control_morale_debug_render prints to a stack buffer where rfg.exe uses its thread-local one | Inferred: the thread-local print buffer is not ported. |
| `src/volition/rfg/code/gameplay/gameflow/gfm_tech_level.cpp:49` | `gfm_tech_level_display` | `0x0079CE90` | The gfm_tech_level console command display prints to a stack buffer where rfg.exe uses its thread-local one | Inferred: the thread-local print buffer is not ported. |
| `src/volition/rfg/code/gameplay/gameplay.cpp:88` | - | `0x016514F0` | Raid_patrol_handle and Raid_squad_handle are external and declared extern in gameplay.cpp, because gameplay_do_frame inlines the raid test in rfg.exe | gameplay_do_frame needs the handles, which rfg.exe reads through its unity build. |
| `src/volition/rfg/code/gameplay/safehouse_vip.cpp:186` | `is_on_restricted_list` | `0x007903C0` | is_on_restricted_list asks activity_vrail_is_riding_shotgun_running() where rfg.exe reads the activity objects directly | Inferred: the activity's state is behind an accessor outside this file. |
| `src/volition/rfg/code/gameplay/upgrades.cpp:745` | `upgrade_has_salvage_collector` | `0x00776420` | upgrade_has_salvage_collector calls wrecking_crew_is_active, where rfg.exe reads the state directly. | Inferred: the call returns the same state rfg.exe reads inline. |
| `src/volition/rfg/code/gameplay/upgrades.cpp:876` | `upgrade_has_heat_seeking_rockets` | `0x00775EA0` | upgrade_has_heat_seeking_rockets asks activity_get_mode() where rfg.exe compares Demolitions_master_activity's mode with the static Current_activity_mode Also: upgrade_has_heat_seeking_rockets compares the activity mode through activity_get_mode instead of the static Current_activity_mode. | The static is private to activity.cpp. |

### rfg/gamewide

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/rfg/code/gamewide/game_save_importer.cpp:67` | `importSaveGameInfo` | `0x007D1410` | importSaveGameInfo reads each directory line into a zero-initialised buffer one byte longer than the line, so it is always terminated; rfg.exe uses an exact 64-byte heap block. Also: importSaveGameInfo reads a save directory line into a buffer one byte larger and zero-terminated | A full 64-byte line in rfg.exe's buffer runs into the next heap block. |
| `src/volition/rfg/code/gamewide/game_time_of_day.cpp:678` | `game_time_of_day_render_debug` | `0x007E9760` | game_time_of_day_render_debug has an empty body, where rfg.exe dereferences rl_getDebugRenderer()'s nullptr when Tod_show_sun_path is on. *Doc claims the null deref is kept; the port body is empty.* | Inferred: the debug renderer is stripped in retail, so the body is dead code. |
| `src/volition/rfg/code/gamewide/gamestate_gameplay.cpp:15` | `gamestate_gp_exit` | `0x007F0D30` | gamestate_gp_exit skips the main scene's partition-system clean when there is no main scene (headless programs); rfg.exe always has one. *The same null check also guards a second clean near line 687 (comment is only in the file header).* | The headless programs have no main scene or render camera. |

### rfg/geom

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/rfg/code/geom/decal_info.cpp:10` | - | - | decal_info.cpp is a split TU: rfg builds the table part, rfg_effects adds the material lookup, getters, release and shutdown with OPENRFG_EFFECTS. | As effects.cpp: programs without the effect library need the tables and name lookups. |
| `src/volition/rfg/code/geom/simple_keyframe.cpp:4` | - | - | The key search of simple_keyframe.cpp is a plain walk where rfg.exe unrolls four keys at a time | It finds the same key. |

### rfg/havok

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/rfg/code/havok/havok_collision_callbacks.cpp:47` | - | - | Contact_event_wrapper (0x02B8CAC0) is defined in havok_collision_callbacks.cpp, its first user in the unity TU, instead of its original location. | Defined at its first user in the unity translation unit. |
| `src/volition/rfg/code/havok/havok_collision_callbacks.cpp:116` | `havok_collide_body_component_index` | - | havok_collide_body_component_index is a new helper for the Havok property-5 component index search that rfg.exe inlines at each use. | Gathers an inlined property search into one function. |
| `src/volition/rfg/code/havok/havok_collision_callbacks.cpp:714` | - | - | The vehicle inverse mass factor for a mass changer is a small helper where rfg.exe inlines it. | Gathers an inlined computation into one function. |
| `src/volition/rfg/code/havok/havok_ragdoll.cpp:305` | `havok_ragdoll_keep_active` | - | havok_ragdoll_keep_active is a new helper in havok_ragdoll.cpp that the human contact handler calls, where rfg.exe sets the ragdoll slot's active stamp inline. | The ragdoll slot type is defined later in the unity TU than the contact handler. |

### rfg/hud

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/rfg/code/hud/fullscreen_map.cpp:468` | `fsm_is_box_obscured_by_fow` | `0x00840150` | fsm_is_box_obscured_by_fow calls fog_of_war_is_posn_obscured, which rfg.exe inlines. | Inferred: the call is the same operation as the inlined code. |
| `src/volition/rfg/code/hud/fullscreen_map.cpp:2094` | - | - | The map's activity reward values (salvage, control, morale) start at 0, where rfg.exe leaves them uninitialized for get_activity_rewards to fill. *Enclosing function not identified.* | Inferred: avoids reading uninitialized values when get_activity_rewards does not fill them. |
| `src/volition/rfg/code/hud/hud_ui_activity_briefing.cpp:225` | `check_for_user_input` | `0x00845550` | Button_input_enabled is read through hud_is_button_input_enabled() because it is hud.cpp's static, which rfg.exe reads directly through the unity file | The static is private to hud.cpp. |
| `src/volition/rfg/code/hud/hud_ui_activity_briefing.cpp:254` | `check_for_user_input` | `0x00845550` | check_for_user_input leaves out a sound choice that cannot run, because Exit is false on that path | The code is unreachable. |
| `src/volition/rfg/code/hud/hud_ui_district_liberation.cpp:92` | `hudl_district_object` | - | hudl_district_object, hudl_leave and the huac_* helpers are separate static functions where rfg.exe inlines them | Inferred: shares the bodies between callers. |
| `src/volition/rfg/code/hud/hud_ui_district_liberation.cpp:301` | `hud_ui_district_liberation_process_header` | `0x00856860` | hud_ui_district_liberation_process_header clears the title for an unknown screen type where rfg.exe would encode an uninitialized buffer | Avoids an uninitialized read; no caller passes another type. |
| `src/volition/rfg/code/hud/hud_ui_mission_briefing.cpp:341` | `process_mission_information` | `0x0084A000` | The mission briefing's process_mission_information drops rfg.exe's third str_make_upper on an unfilled buffer. | That call reads an uninitialized stack buffer that nothing fills or reads. |
| `src/volition/rfg/code/hud/hud_ui_mission_complete.cpp:194` | `humc_get_district` | - | humc_get_district and humc_get_morale_percent are static helpers where rfg.exe inlines them | Inferred: shares the bodies between callers. |
| `src/volition/rfg/code/hud/in_world_gps.cpp:154` | `in_world_gps_register_path` | `0x0084BCA0` | in_world_gps_register_path starts the terrain height at 0.0f, where rfg.exe uses an uninitialized stack slot when the terrain has no answer. | The uninitialized slot holds a pointer's bits in rfg.exe, with no defined value. |
| `src/volition/rfg/code/hud/in_world_gps.cpp:236` | `in_world_gps_update_render` | `0x00869790` | in_world_gps_update_render sets the chevron spline knot_parameter_offset to 0.0f, where rfg.exe leaves an uninitialized stack slot. | The uninitialized slot has no defined value. |

### rfg/interface

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/rfg/code/interface/loading.cpp:997` | `game_boot_thread_stop` | `0x00888DF0` | game_boot_thread_stop does not wait for the boot thread when none was started (Boot_render_thread == 0xFFFF); rfg.exe always waits. | Programs whose first boot stage is a stand-in never start the boot screen thread. |
| `src/volition/rfg/code/interface/main_menu.cpp:210` | - | - | g_openrfgMainMenuNoTitle (--notitle, implied by --host/--join/--sp) lets the main menu catch the local player at once instead of waiting for a controller press. *Set in src/openrfg/main_options.cpp, which also implies it for --sp and --sp-load.* | Test and command line launches must skip the title screen (UI deviation 4). |
| `src/volition/rfg/code/interface/main_menu.cpp:211` | - | - | g_openrfgMainMenuProcess is a hook run at the end of each main menu process, used by OpenRFG to press the system link Host/Join or show a bad command line. *Driver is src/openrfg/main_ui_command_line.cpp.* | Drives the main menu from the command line without user input (UI deviation 4). |
| `src/volition/rfg/code/interface/main_menu.cpp:1506` | `main_menu_process` | - | --notitle catches the local player at once in main_menu_process; --host/--join imply it. *The comment is on the seam definition at main_menu.cpp:207, not at this line.* | Skip the title screen for command line launches (UI deviation 4). |
| `src/volition/rfg/code/interface/option_menus/cheats_menu.cpp:233` | `option_change_occurred` | `0x008FCF90` | cheats_menu option_change_occurred calls ui_add_msgbox, which rfg.exe inlines. | Inferred: the call is the same operation as the inlined code. |
| `src/volition/rfg/code/interface/option_menus/extras_menu.cpp:143` | `extras_create_menu` | `0x008E6FF0` | extras_create_menu selects the first item with ui_menu_set_selected_item, where rfg.exe sets m_selected directly. | Inferred: the call writes the same field the original sets inline. |
| `src/volition/rfg/code/interface/ui_document_manager.cpp:895` | `ui_reload_bitmat_sheet_table_file` | `0x008C71A0` | ui_reload_bitmat_sheet_table_file clears stale copies of a deleted sheet name past the new end so they are not deleted twice, where rfg.exe double-frees them. | Inferred: avoids a double free. |
| `src/volition/rfg/code/interface/ui_document_menu_wrecking_crew.cpp:813` | `wcmm_process_input` | - | wcmm_process_input starts its consumed flag at false, where rfg.exe uses whatever ECX held. | Avoids an uninitialised value; the function has no caller. |
| `src/volition/rfg/code/interface/ui_document_menu_wrecking_crew_character_select.cpp:147` | `wccs_stream_in_image` | - | Streamed_image_name_uiwccs has 5 entries, not 4, because wccs_stream_in_image accepts a fifth name that overruns rfg.exe's array. | Avoids the out-of-bounds write. |
| `src/volition/rfg/code/interface/ui_document_menu_wrecking_crew_scoreboard.cpp:806` | `wc_scoreboard_process_data_items` | `0x008EED60` | The Wrecking Crew scoreboard passes dtime 0 to its data item processors, where rfg.exe passes stack garbage. | The functions ignore dtime; 0 avoids an uninitialised value. |

### rfg/level

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/rfg/code/level/world.cpp:1785` | `world::set_world_state` | `0x00753B40` | set_world_state also writes a game trace line, and get/set_world_state are used where rfg.exe touches cur_world_state directly *Both functions carry rfg.exe addresses now, so they may exist in rfg.exe; only the trace line is certainly a deviation.* | Inferred: the trace line is the port's debugging aid (--game-trace). |
| `src/volition/rfg/code/level/world_serialize.cpp:1501` | `platform_save_to_disk` | `0x0092D770` | platform_save_to_disk falls back to district index 0 when the save position is outside every district, where rfg.exe dereferences the null district without a check. Also: platform_save_to_disk checks the district at the safehouse or player position for null and uses index 0 | Inferred: a position outside every district would crash the save. |

### rfg/multi

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/rfg/code/multi/multi_core/generic/multi_core_platform_generic.cpp:643` | `multi_platform_init_my_uid` | `0x0098E100` | The LAN uid carries the --lan-port value as its port, where rfg.exe leaves it 0, so instances on one machine get different uids. Also: The LAN player uid (My_player_uid) carries the local LAN port in the address, and uses the route-based own address; rfg.exe leaves the port 0. | Several instances on one machine must be distinguishable for LAN play. |
| `src/volition/rfg/code/multi/multi_core/generic/multi_core_platform_generic.cpp:679` | `multi_core_platform_internal_scoreboard_init` | `0x0098E280` | multi_core_platform_internal_scoreboard_init keeps the link's LAN uid when no user account exists, instead of invalidating My_player_uid. Also: multi_core_platform_set_my_player_uid keeps the uid set by the session start instead of invalidating it when no account is signed in. | rfg.exe always has a signed-in Steam user whose id replaces the uid; clearing it left the host's connection unmatched and best host selection never ended. |
| `src/volition/rfg/code/multi/multi_core/global/multi_core_session_base.cpp:1224` | `multi_session_base::check_modified_table_hash` | `0x009F7AB0` | Off by default, g_openrfgSkipSystemLinkTableHashCheck makes check_modified_table_hash return early for LAN sessions, where rfg.exe deletes the session at match start. Also: check_modified_table_hash returns early for system link sessions when g_openrfgSkipSystemLinkTableHashCheck is set; rfg.exe always compares the table tag. *Set to true by src/openrfg/main_options.cpp:345.* | The hash check deletes a LAN session at the match start (README MP4 row). |
| `src/volition/rfg/code/multi/multi_core/global/multi_core_session_base.cpp:1225` | `multi_session_base::check_modified_table_hash` | `0x009F7AB0` | g_openrfgSkipSystemLinkTableHashCheck is set by the driver and makes check_modified_table_hash return early for LAN sessions. *src/openrfg/main_options.cpp sets it unconditionally.* | rfg.exe ends a LAN match at its start. |
| `src/volition/rfg/code/multi/multi_core/global/multi_core_session_base.cpp:2302` | `multi_session_base::add_connection` | `0x009AA3A0` | add_connection marks used indices in a 256-entry array, where rfg.exe uses 16 bytes so an index of 0xFF writes past it. | Inferred: avoids a stack overrun for index 0xFF. |
| `src/volition/rfg/code/multi/multi_game/modes/multi_game_mode_bagman.cpp:1022` | `bagman_mode_hud_process` | `0x00991740` | bagman_mode_hud_process fills the sorted handles array with 0xFF (invalid), where rfg.exe leaves it uninitialized and reads it when there are no players. | Inferred: avoids reading uninitialized stack data. |
| `src/volition/rfg/code/multi/multi_game/multi_game_backpack_rack.cpp:256` | `multi_game_backpack_rack_round_begin` | `0x009DC6E0` | The round begin's backpack rack groups and types arrays (32 groups x 128) are function-local statics instead of stack arrays. *Same for the types array; no comment marks the static as a deviation.* | Inferred: the arrays are large (32 x 128 entries) and would be heavy on the stack. |
| `src/volition/rfg/code/multi/multi_game/multi_game_object_backpack.cpp:491` | `ripple_process_players_hit` | `0x009F14C0` | The ripple's group knockdown experience event calls the public multi_game_xp_trigger_event; rfg.exe inlines the repeat timer, trigger and server PT_XP. *multi_game_xp.cpp is now fully ported but the stand-in call remains.* | The experience system was outside the multiplayer scope, so the public trigger stands in. |
| `src/volition/rfg/code/multi/multi_game/multi_game_player.cpp:1000` | `multi_game_player_process_state` | - | Client-side regeneration delay started from synced hit points (human_health_damage_register_from_network). | Without it the client's regeneration faded all blood decals at once. |
| `src/volition/rfg/code/multi/multi_game/multi_game_player.cpp:1001` | `multi_game_player_process_state` | - | human_health_damage_register_from_network is called from multi_game_player_process_state when synced hit points drop, starting the client's regen delay. | The client otherwise regenerates between syncs and fades the decals at once. |
| `src/volition/rfg/code/multi/multi_game/multi_game_stats.cpp:447` | `multi_game_stats_process_player_stats` | - | A stats packet with an invalid handle is read into a zero-initialised scratch copy, where rfg.exe reads into an uninitialised stack copy. *Code is in read_player_stat_data_from_packet, not the listed function; no va comment there.* | Inferred: deterministic discarded values instead of uninitialised stack data. |

### rfg/objects

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/rfg/code/objects/humans/human.cpp:4609` | `human_health_damage_register_from_network` | `0x00ACED70` | Port adds human_health_damage_register_from_network so a multiplayer client starts the regeneration delay that rfg.exe's human_damage skips there. Also: A multiplayer client starts the health regeneration delay when synced hit points drop, via human_health_damage_register_from_network. | Without it regeneration runs every frame and a hit player's blood decals fade at once on clients. |
| `src/volition/rfg/code/objects/humans/human.cpp:4614` | `human_health_damage_register_from_network` | - | On a multiplayer client a hit starts the regeneration delay through new human_health_damage_register_from_network, which rfg.exe never runs there. *Called from multi_game_player.cpp line 1001.* | Otherwise the client regenerates every frame and the blood decals fade at once. |
| `src/volition/rfg/code/objects/humans/human.cpp:11276` | `human::update_render` | `0x00A9BB90` | human::update_render starts the 40 morph weights at zero, where rfg.exe leaves them uninitialised when there are neither body nor head morphs. | Inferred: avoids activating morphs from stack garbage. |
| `src/volition/rfg/code/objects/humans/human_combat.cpp:288` | `parse_melee_table` | `0x00AB2A80` | parse_melee_table allocates 4 extra bytes for the array count cookie, where rfg.exe allocates count*0xDC and writes the count past the block. | Inferred: avoids a 4-byte heap overrun. |
| `src/volition/rfg/code/objects/humans/human_heads_and_props.cpp:562` | `human_hap_process_morphs` | `0x00A7F9E0` | human_hap_process_morphs calls render_distance.get_apparent_distance, which rfg.exe inlines. | Inferred: the call is the same operation as the inlined code. |
| `src/volition/rfg/code/objects/humans/player.cpp:1006` | `player::do_frame_blood_decals` | `0x00A5C7D0` | Player blood decals fade by Player_blood_decal_fade_rate * Frametime * 60 per frame instead of a fixed step per frame. | At ~240 Hz the per-frame fade made decals vanish in under 1 s, before the bloodsplat shader's reveal completed. |
| `src/volition/rfg/code/objects/humans/player.cpp:1009` | - | - | The player's blood decal fade step is scaled by Frametime * 60, where rfg.exe subtracts the rate every frame. Also: Frame-time based blood decal fade (rate * Frametime * 60). Also: player::do_frame_blood_decals subtracts the fade rate times Frametime times 60. *Enclosing function name not looked up.* | Frame-rate independence: the original fade finishes in under 1 s at 240 Hz. |
| `src/volition/rfg/code/objects/humans/player.cpp:4945` | `player_is_near_demolitions_master_node` | `0x00A5BF40` | player_is_near_demolitions_master_node leaves out the node walk rfg.exe does while the activity cannot start, whose two distances it drops | The walk's results are discarded. |
| `src/volition/rfg/code/objects/humans/player.cpp:5186` | `player_is_near_action_node` | `0x00AC1DF0` | player_is_near_action_node starts its entry point at ZERO_VECTOR, where rfg.exe reads an uninitialized stack slot the first time. | The uninitialized slot has no defined value. |
| `src/volition/rfg/code/objects/humans/player_hold_object.cpp:430` | `player_hold_render_debug` | `0x00A11930` | player_hold_render_debug never draws the held object's mass text. | gr_string has no target here, and the flag is false in rfg.exe anyway. |
| `src/volition/rfg/code/objects/ladder.h:52` | `ladder::is_enabled` | - | ladder.h has a small accessor is_enabled for a field rfg.exe reads directly | The field is private in the port, and human_move_pathfind.cpp reads it |
| `src/volition/rfg/code/objects/movers/mover_fader.cpp:88` | `transfer_decals` | `0x00A381A0` | transfer_decals skips a missing owner mover; rfg.exe calls transfer_decals_to_fader on a null mover. | The original makes a null read when the owner is gone. |
| `src/volition/rfg/code/objects/object_cover_node.h:71` | - | - | object_cover_node.h has a get_disabled_timer accessor for a field rfg.exe reads directly; ladder.h accessor not checked. *ladder.h accessors not looked at.* | Inferred: lets spawn_placement.cpp read the timer through a public accessor instead of a private field. |
| `src/volition/rfg/code/objects/object_guard_node.cpp:324` | `object_guard_node::serialize` | `0x00AA7060` | object_guard_node::serialize leaves a missing or wrong-size property at the value just set, where rfg.exe reads an uninitialized stack slot. | The uninitialized read has no defined value. |
| `src/volition/rfg/code/objects/object_item.cpp:103` | - | - | The money pickup message buffer is a file-local __declspec(thread) array, where rfg.exe uses its per-thread buffer (TLS +0x38). | rfg.exe's per-thread data block is not ported, so the buffer stands alone. |
| `src/volition/rfg/code/objects/object_item.cpp:835` | `item_touch_give_cash` | - | item_touch_give_money and item_touch_give_money2 share a new helper item_touch_give_cash, where rfg.exe has the same instructions in both after the amount. | The doc says rfg.exe has identical code in both; the port factors it into one helper. |
| `src/volition/rfg/code/objects/object_roadblock_node.cpp:99` | `clean_up_roadblock_mover` | `0x00A1BF00` | The three clean_up_roadblock_mover overloads and destroy_all_child_objects take normal arguments where LTCG used registers; the object* and handle overloads have no caller | The port cannot express rfg.exe's LTCG register calling convention. |
| `src/volition/rfg/code/objects/object_roadblock_node.cpp:152` | `object_roadblock_node::cleanup_roadblock` | `0x00A6AAB0` | cleanup_roadblock calls clean_up_roadblock_mover for the world's movers too and shares defer_mover_clean_up, where rfg.exe inlines both | Inferred: shares the bodies between callers. |
| `src/volition/rfg/code/objects/object_roadblock_node.cpp:898` | - | - | road_block_road, assign_orders, get_child_vehicle_handle, start and destroy_all_child_objects are called as functions where rfg.exe inlines them | Inferred: shares the bodies between callers. |
| `src/volition/rfg/code/objects/object_roadblock_node.cpp:1028` | `object_roadblock_node::place_bbox` | `0x00A3DC40` | place_bbox uses obj_prop_block(pos, orient, nullptr), which writes the same OBJ_PROP_COMBINED rfg.exe writes by hand | Same effect through the constructor. |
| `src/volition/rfg/code/objects/object_roadblock_node.cpp:1207` | - | - | do_frame_clean_up uses remove_unsorted for the inlined swap | Same effect through the container method. |
| `src/volition/rfg/code/objects/object_safehouse.cpp:103` | `object_safehouse_handle_enable` | `0x00AAB580` | object_safehouse_handle_enable and _disable are external, where rfg.exe has them file-local | Friend declarations need external linkage. |
| `src/volition/rfg/code/objects/object_upgrade_node.cpp:98` | `object_upgrade_node::serialize` | - | object_upgrade_node::serialize starts with an empty name buffer where rfg.exe compares it uninitialized | An empty name matches no node name, avoiding an uninitialized read. |
| `src/volition/rfg/code/objects/salvage.cpp:195` | `salvage_render_debug` | `0x00A724D0` | salvage_render_debug prints to a stack buffer where rfg.exe uses its thread-local one | Inferred: the thread-local print buffer is not ported. |
| `src/volition/rfg/code/objects/turret.cpp:186` | `turret::autoaim` | `0x00A95FF0` | turret::autoaim does not build the matrix that Turret_autoaim_debug would feed to debug drawing. | It only served debug drawing and is stripped from retail. |
| `src/volition/rfg/code/objects/turret.cpp:1432` | - | - | Int3 critical-function prints in turret.cpp print a fixed string, without the file and line rfg.exe formats. *The same line repeats in turret_internal.cpp (several sites).* | Debug-only formatted message shortened. |
| `src/volition/rfg/code/objects/turret_info.cpp:47` | `parse_turret_flag` | - | parse_turret_flag ignores an unknown flag silently, where rfg.exe formats a debug message. | Debug-only message; the doc drops it as it has no effect on behaviour. |
| `src/volition/rfg/code/objects/turret_internal.cpp:422` | `turret_physics_stream_load` | `0x00A44470` | The missing mount tag message for a secondary weapon is not formatted or printed. *The function at 0x00A44470 is the static one that sets secondary_weapon_attach_component; its name was not read. The missing grip tag message was not found.* | Debug-only formatted message dropped. |
| `src/volition/rfg/code/objects/turret_internal.cpp:900` | `create_item_component` | `0x00A736F0` | create_item_component calls set_is_visible and set_lod_level on the mesh instance, where rfg.exe writes the fields inline. *Doc says only set_opacity and set_is_visible had their bodies read; set_lod_level and get_local_aabb are unverified equivalents.* | Inferred: the member setters stand for the original's inlined field writes. |

### rfg/scripting

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/rfg/code/scripting/action_script/script_definitions.cpp:3977` | - | - | script callbacks call critical_area_delete, scripted_activity_courier_escape and similar as separate functions where rfg.exe inlines them | Inferred: shares the bodies between callers. |
| `src/volition/rfg/code/scripting/action_script/script_definitions.cpp:4367` | `set_npc_max_hit_points` | `0x00B22420` | set_npc_max_hit_points, set_squad_max_hit_points and set_complain_flag_squad take normal arguments where LTCG used registers | The port cannot express rfg.exe's LTCG register calling convention. |
| `src/volition/rfg/code/scripting/action_script/script_definitions.cpp:4456` | `Action_callback_trigger_region_create` | `0x00B28800` | trigger_region_create calls create_object<trigger_region> with a null-zone obj_prop_block, as rfg.exe's direct call does *The doc calls this a deviation but also says the result is the same.* | Same behaviour through the generic template. |
| `src/volition/rfg/code/scripting/objective_highlight.cpp:88` | `glow_set_rl_outline` | `0x00B2DA70` | The mover paths call the virtual get_render_model_instance instead of rfg.exe's inlined read per mover subtype *The call is in the OT_MOVER case here and again in clear_glow_effect; the anchor is the function's header comment since the call line repeats.* | Inferred: the virtual returns the same instance, so the per-subtype reads need not be repeated. |
| `src/volition/rfg/code/scripting/objective_highlight.cpp:95` | `glow_set_rl_outline` | `0x00B2DA70` | glow_set_rl_outline and clear_glow_effect are external functions here, where rfg.exe has them file-local | The object_effect and object_mover friend declarations need external linkage. |
| `src/volition/rfg/code/scripting/objective_highlight.cpp:181` | `clear_glow_effect` | `0x00B33710` | glow_cleanup calls clear_glow_effect, which rfg.exe inlines with the same body | Inferred: avoids duplicating the body. |
| `src/volition/rfg/code/scripting/objective_highlight.cpp:305` | `create_glow_effect` | `0x00B33930` | create_glow_effect adds the handle to the glow list without the full-list Int3 message rfg.exe prints and drops the entry for; the port's add just refuses it. *Did not read the add implementation to confirm it refuses silently.* | Inferred: rfg.exe's Int3 debug message is not reproduced; the add call handles a full list. |
| `src/volition/rfg/code/scripting/objective_highlight.cpp:336` | `objective_highlight_target_marker_update_render_cb` | `0x00B3DA60` | The three marker render callbacks (target, activity, ground) are external here, where rfg.exe has them file-local | The object_effect and object_mover friend declarations need external linkage. |
| `src/volition/rfg/code/scripting/objective_highlight.cpp:379` | `objective_highlight_target_marker_update_render_cb` | `0x00B3DA60` | A marker's position is zero-initialized where rfg.exe leaves it unset, in the Int3 path that objective_highlight_do_frame never lets through | Avoids reading an uninitialized vector in a path that is never reached. |
| `src/volition/rfg/code/scripting/public_address.cpp:550` | `public_address_group_play_queued_lines` | - | public_address_group_play_queued_lines is a separate function called from the group update, where rfg.exe inlines it | Inferred: keeps the update function readable. |

### rfg/spawn

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/rfg/code/spawn/spawn_placement.cpp:1426` | `spawn_npc` | `0x00B48330` | spawn_npc creates the npc through World.create_object<npc> where rfg.exe calls create_object_internal directly. *A later comment at the site mentions create_object_internal but only for the kept bug.* | Inferred: the typed template stands in for the direct internal call. |
| `src/volition/rfg/code/spawn/spawn_placement.cpp:1948` | `do_npc_spawn_location` | `0x00B5BC20` | The nav cell search node pool in do_npc_spawn_location is a 512-node stack array, where rfg.exe takes 0x4000 bytes from the thread's temporary mempool. | Stack array stands in for the temporary mempool allocation. |

### rfg/vehicle

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/rfg/code/vehicle/vehicle_class.cpp:488` | `vehicle_turret_turn_off_components` | `0x00BA8850` | vehicle_turret_turn_off_components stops the parent walk when parent is -1, where rfg.exe goes on and reads the component before the first. Also: vehicle_turret_turn_off_components stops walking at parent -1 instead of reading the component before the first. | Inferred: avoids an out-of-bounds read of m_components[-1]. |

### rfg/weapons

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/rfg/code/weapons/arc_welder.cpp:598` | `arc_welder_process_alr` | `0x00BD93C0` | arc_welder_process_alr passes a null position for a null player instead of rfg.exe's invalid pointer. | Avoids passing an invalid pointer. |
| `src/volition/rfg/code/weapons/arc_welder.cpp:701` | - | `0x00BE96E0` | An npc owner reaching the arc welder's per-frame code returns when the player pointer is null, where rfg.exe calls get_equipped_weapon on null. Also: process_arc_welder returns when the owner player is null instead of calling get_equipped_weapon on it. *VA is the nearest preceding VA comment (line 648); enclosing function name not confirmed.* | rfg.exe would crash if ever reached; npcs do not carry arc welders. |
| `src/volition/rfg/code/weapons/grinder.cpp:364` | `weapon::grinder_update_render` | `0x00BDB730` | grinder_update_render sets y, z and w of the mesh instance parameters to 0, where rfg.exe uses an uninitialized local (y a stale instance count). | Inferred: avoids uninitialised stack data. |

### rfg/weather

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/rfg/code/weather/weather.cpp:12` | - | `0x038B2F90` | weather.cpp is a split TU: rfg builds only Weather_stats, rfg_effects builds the weather system with OPENRFG_EFFECTS. | The undergrowth's wind reads Weather_stats in every program. |
| `src/volition/rfg/code/weather/wind.cpp:15` | - | - | wind.cpp is a split TU: rfg builds only weather_wind_get_speed, rfg_effects builds the wind ramps with OPENRFG_EFFECTS. | The undergrowth reads the wind speed in every program. |

### tests

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/openrfg/main_audio_log.cpp:1` | - | - | The --audio-log option writes a line per audiolib_cue_play result to a file; rfg.exe has no such option. *Built only with OPENRFG_AUDIO; option parsed in src/openrfg/main_options.cpp.* | Test seam to observe which cues play (SFX.md deviation 3.1), as retail data has no cue names. |
| `src/openrfg/main_options.cpp:1` | - | - | OpenRFG's command line (--host, --join, --sp, --save-dir, --notitle, --no-steam, --no-steam-cloud-saves, --stub-trace, --game-trace, --audio-log) is new code in src/openrfg. *Still in src/openrfg; src/openrfg/main does not exist.* | Test and launch seams absent from rfg.exe. |
| `src/openrfg/main_options.cpp:58` | `add_user_name` | - | A command line (main_options) is parsed by a static initializer in main_hooks.cpp; --name is injected as -username. | Launch options for host, join, name, start position and so on; keen's parseArguments reads -username. |
| `src/openrfg/main_options.cpp:70` | `add_user_name` | - | --name adds -username NAME to the argument vector so the user account stub has one local account; rfg.exe uses the Steam user. | Inferred: without Steam there is no account to name the local player or lobby entry. |
| `src/openrfg/main_options.cpp:200` | - | - | OpenRFG adds --sp and --sp-zone NAME options that start a single-player game in a zone, the same call as video_cutscene_exit_state but without the movie | Inferred: lets the test program start the single-player game directly without the intro movie. |
| `src/openrfg/main_options.cpp:221` | `main_options_parse` | - | OpenRFG has a multiplayer command line (--map, --players, --ip, --port, --name, --host, --join, limits, --search, --results-time) that rfg.exe lacks; --name becomes -username. *Documented in main_options.h and the file header.* | Lets the original UI flow be driven for multiplayer without clicking and test instances be launched. |
| `src/openrfg/main_options.cpp:349` | `main_options_parse` | - | OpenRFG sets g_openrfgSkipSystemLinkTableHashCheck to true at option parsing, so a system link match is not ended by the table hash check. *Set unconditionally, not only for lobby-driver runs.* | Without it a system link match ends at its start (multiplayer.md MP4). |
| `src/openrfg/main_options.cpp:354` | - | - | --notitle, --host, --join, --sp and --sp-load skip the startup videos and the title screen. *Seam variables live in main_menu.cpp and ui_startup_video.h.* | Lets automated runs reach the game without the videos (UI.md deviation 4). |
| `src/openrfg/main_options.cpp:381` | `main_options_choose_lan_port` | - | main_options_choose_lan_port picks the LAN port automatically (--port for a host, the first free UDP port up to 16 above it for a client). Also: A joining instance binds --port if free, else the next free UDP port above it (up to 16); the host binds --port. rfg.exe has one fixed port. *File header names deviation 3 as new code.* | Several instances on one machine need distinct LAN ports (orig_main.md deviation 3). |
| `src/openrfg/main_options.cpp:398` | `main_options_direct_lobby` | - | main_options_direct_lobby replaces a loopback or own --ip address with the machine's own network address for a direct join. Also: A loopback or own-machine --ip for a direct join is replaced by the machine's own default-route network address. | The LAN link lists a client joined over 127.0.0.1 under another address and drops it (multiplayer.md MP2). |
| `src/openrfg/main_ui_command_line.cpp:124` | - | - | The LAN port is chosen at the Host/Join press by the command line driver. | Instances on one machine need distinct LAN ports. |

### vfxlib

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/vfxlib/code/vfx.cpp:23` | - | `0x0108FB50` | vfx.cpp uses #pragma init_seg(lib) so class_factories is constructed before the emitter and filter factory infos append themselves. | rfg.exe compiles vfxlib as one unity TU; here the files are separate TUs and the list constructor would otherwise clear appended entries. |
| `src/volition/vfxlib/code/vfx.cpp:28` | - | - | vfx.cpp forces the ten emitter and filter factory infos into the link with #pragma comment(linker, "/include:..."). *Ten /include lines present.* | Nothing references the infos by name, so a static library link drops them and every template using them fails to load. |
| `src/volition/vfxlib/code/vfx_effect.h:29` | - | - | vfx_effect, vfx_effect_template, the emitter and instance classes and object_effect befriend the tracer functions, with an opaque 'enum weapon_tracer_type;' declaring the parameter type. *Same declaration in the emitter, emitter instance, template and object_effect headers.* | The tracer code pokes vfxlib internals that rfg.exe reaches through its unity build. |

### vlib

| Location | Function | rfg.exe | Deviation | Why |
|---|---|---|---|---|
| `src/volition/vlib/code/bmpman/bmpman.cpp:760` | `bm_load_peg` | - | bm_load_peg writes the GPU extension 'g' into a static 2-byte buffer instead of a string literal when the name has no extension. | rfg.exe writes into a read-only string literal; the port writes into a static buffer. |
| `src/volition/vlib/code/bmpman/bmpman.cpp:866` | `bm_load_peg_header` | `0x005CE230` | bm_load_peg_header appends an empty string for a name without extension, where rfg.exe reads one byte past the "" literal and appends whatever follows. Also: bm_load_peg_header appends an empty string for a name with no extension, where rfg.exe reads past the "" literal. | Inferred: avoids an out-of-bounds read of a string literal. |
| `src/volition/vlib/code/bmpman/bmpman.cpp:931` | `bm_save_peg` | `0x005CFE70` | bm_save_peg writes the extension 'g' into a static buffer for a name without extension, where rfg.exe writes it past the end of a "" literal. Also: bm_save_peg writes the GPU extension 'g' into a static buffer instead of past the end of a "" literal. | Inferred: avoids writing past a string literal (undefined behaviour) while keeping the output identical. |
| `src/volition/vlib/code/bmpman/peg_assemble.cpp:236` | `peg_asm_add` | `0x005B5580` | peg_asm_add zero-initialises the name and vbm_name buffers, which rfg.exe leaves uninitialised for an entry without a name. Also: peg_asm_add zeroes the name and vbm_name buffers that rfg.exe leaves uninitialised for a nameless entry. | Inferred: avoids using uninitialised stack data when cf_drop_ext/cf_add_ext return early. |
| `src/volition/vlib/code/bmpman/peg_assemble.cpp:344` | `peg_asm_find_bitmap` | `0x005B5870` | peg_asm_find_bitmap zero-initialises the name buffer, which rfg.exe leaves uninitialised for a null name. Also: peg_asm_find_bitmap zeroes its name buffer, which rfg.exe leaves uninitialised for a null name. | Inferred: avoids using uninitialised stack data in the hash lookup. |
| `src/volition/vlib/code/cfile/cfile.cpp:773` | `cf_read_compressed` | `0x005C7EB0` | cf_read_compressed passes chain arrays padded with an empty second entry, where rfg.exe passes one-entry chains built from the addresses of its arguments. Also: cf_read_compressed passes chain arrays padded with an empty second entry, where rfg.exe passes the addresses of its two arguments as one-entry chains. | cf_decompress_chain reads the entry after a filled buffer, which in rfg.exe is whatever follows on the stack. |
| `src/volition/vlib/code/cfile/keen_cfile.cpp:355` | `keen_cf_read_packfile` | `0x005B6A80` | keen_cf_read_packfile passes chain arrays padded with an empty second entry, where rfg.exe passes addresses of dest_buf and a local as one-entry chains. | cf_decompress_chain looks one entry ahead, so the padding keeps it from reading past the caller's arguments. |
| `src/volition/vlib/code/cfile/packfile.cpp:452` | `packfile_read_condensed` | `0x005CAC20` | packfile_read_condensed passes chain arrays padded with an empty second entry, where rfg.exe passes addresses of two locals as one-entry chains. | cf_decompress_chain looks one entry ahead, so the padding keeps it from reading past the caller's locals. |
| `src/volition/vlib/code/os/vconsole.cpp:1384` | - | - | Entering a command checks Command_history_last > -1 before comparing with the last history entry; rfg.exe compares first and reads before the array on an empty history. *Comment is in the file header notes; enclosing function (command entry) has no VA comment.* | rfg.exe reads the memory before the history array. |
| `src/volition/vlib/code/os/vconsole.cpp:1621` | `console_process_gr` | `0x005CC320` | console_process_gr treats typed key codes of 256 and above (modifier bits) as released instead of indexing Key_states out of bounds as rfg.exe does. *The comment is in the file header notes, not at the line.* | rfg.exe reads unrelated memory there. |

## No longer in the code

A doc lists these, but the code has since moved back to `rfg.exe`'s behaviour or was deleted.

| Doc | Deviation | Why it was made | What was found |
|---|---|---|---|
| `UI.md:160` | hud_ui_is_loaded was answered by a placeholder deviation until the HUD documents' bitmaps existed (OM7 deviation 3). | The placeholder HUD stood in before the HUD documents were ported. | ui_document_hud.cpp hud_ui_is_loaded now returns Cant_load \|\| (!Unloaded && Data_items_created_uihud) with no port-specific branch. |
| `UI.md:257` | render_draw_video and render_draw_map were stand-ins for the video player and the cartographer. | The video player and cartographer were not yet ported when vint_rfg.cpp was. | vint_rfg.cpp calls video_player_render (video_player.cpp) and Cartographer.render (hud/minimap.cpp), both real. The comment above render_draw_map still says 'stand-in until then' (stale). |
| `UI.md:263` | A --ui-log seam (UI deviation 5) for the UI tests. | Test support for the UI screens. | No ui-log symbol or option anywhere in src/ or tests/; the seams driver tests were deleted. |
| `UI.md:276` | game_save_has_save_data was a stand-in returning true at once in OpenRFG (main_stubs_game.cpp). | There was no save system yet. | Real game_save_has_save_data in gamewide/game_save_generic.cpp, called by main_menu_wait_for_options; no stand-in left in src/openrfg. |
| `UI.md:276` | LocalGameSession::addLocalPlayer was a generated stub that added no user. | Generated stub before the session code was ported. | Real implementation in localgamesession.cpp, called from main_menu.cpp. The uid consequence is still handled by the UI5 deviation in multi_core_platform_set_my_player_uid. |
| `UI.md:281` | The video player was not ported (UI deviation 2). | Deferred at the time of UI5. | video_player.cpp exists with video_player_render and video_player_render_all. |
| `UI.md:302` | ui_character.cpp was the UI4 stand-in for the main menu's Mason. | Real character not ported at UI6. | ui_character.cpp now draws the real Mason with hammer, coat and head (final_push step 6). |
| `UI.md:302` | Scoreboard, matchmaking, party, Live, tutorial menus and multi_game_xp/badge/party/option-name functions were generated stubs. | Not yet ported at UI6. | src/stubs/rfg/interface/interface_multi.cpp and interface_main.cpp hold no function definitions; no multi_game_xp_/badge_/party_ stub anywhere in src/stubs. |
| `UI.md:317` | Quit Game in the Esc menu skipped the MENU_QUIT_CONFIRM_MP box in the placeholder (OM7 deviation). | The placeholder in-game options menu did the quit directly. | Original callback and ingame_options_confirm_msgbox_cb are in place, adding the confirmation box; no placeholder files remain in src. |
| `UI.md:322` | ui_ingame_options_placeholder.cpp and the 2026-10-26 Quit Game skip of the confirmation box. | Placeholder deleted once the original Esc menu was ported. | Glob for *placeholder* under src finds nothing; mainmenu_quit_to_mainmenu_callback shows the box. |
| `UI.md:329` | hud_ui_is_loaded returned a placeholder value and ui_hud_placeholder.cpp existed (deviation 3). | Placeholder HUD before the original HUD documents. | Placeholder file gone, function matches rfg.exe. |
| `VFX.md:122` | OpenRFG --screenshot FRAME FILE and an env-gated --play-effect probe in the viewers (test seams). | Test support for effects (orig_mvp.md deviation 1). | No --screenshot or --play-effect option in src/openrfg; viewers deleted. A different seam, g_openrfgRenderFrameDone (application.hpp), remains for test screenshots. |
| `VFX.md:264` | vehicle_get_component_index_from_shape_key was a STUB returning -1 in src/stubs/render_game_seams.cpp, so vehicle decals went on the vehicle's own instance. | Vehicle code was not ported when the decals were. | Real function at vehicle_class.cpp:5241; render_game_seams.cpp is gone. |
| `final_push.md:309` | Per-character backpack offsets in multi_game_object_backpack.cpp were not read (MP10 deviation). | Was an MP10 simplification. | Retired; code reads the offsets. |
| `final_push.md:337` | get_player_offset ignored the Wrecking Crew player's character offsets (MP10 deviation). | Was an MP10 simplification. | Retired; code reads the offsets. |
| `final_push.md:493` | hud_ui_is_loaded returned a placeholder value (UI.md deviation 3, OM7) while the HUD documents' bitmaps did not exist. | Placeholder HUD stood in until the real HUD was ported. | Now returns Cant_load \|\| (!Unloaded && Data_items_created_uihud); UI.md says it is rfg.exe's value again. Not re-verified against the binary here. |
| `final_push.md:82` | Per-character Wrecking Crew backpack offsets were not read in multi_object_backpack::get_player_offset. | Was an MP10 simplification. | get_player_offset now reads wrecking_crew.xtbl offsets of the current player (line 1899). |
| `gameflow.md:185` | hud_ui_handbook save and load once kept the section's data as read and wrote it back; they now save and load the real node records | The pass-through was only for as long as the handbook was not ported. | Both functions are real ports now (pool walk, name crc, viewed, unlock number); no pass-through left. |
| `gameflow.md:185` | public_address_kiosk save and load once kept the section's data as read and wrote it back; they now save and load speakers, kiosks and lines | The pass-through was only for as long as the public address system was not ported. | Both functions are real ports now; no pass-through left. A load in one run still adds to the level's registered speakers, as in rfg.exe. |
| `gameflow.md:185` | fullscreen_map_init was a stand-in that only called fog_of_war_init; it is now the full function | The map was ported later. | Now calls load_objectives, fog_of_war_init, fullscreen_map_terr_info_init and registers the state. |
| `gameflow.md:213` | Player_kills_hostage_morale_penalty used to live in gfm_control_morale.cpp; it is now defined in activity_house_arrest.cpp and morale code only declares it extern | Its table entry sits with the house arrest activity. | Defined at activity_house_arrest.cpp:71 as in rfg.exe's layout. |
| `gameflow.md:273` | RESTRICTED_ACTIVITY_LIST and REGISTER_DAMAGE_ACTIVITY_LIST held modes so a slot could stay null; they now hold activity objects as rfg.exe does | The stand-in was only needed until each activity was ported. | Both lists hold objects (activity_register_damage.cpp:53 too). Confirmed by gameflow.md:589. |
| `gameflow.md:273` | activity_none, activity_disable_mayhem_for_tutorial, activity_squad_destroy_notify and the start paths went through All_activities with a null check; they now name the objects directly | The null-slot stand-in was only needed until each activity was ported. | activity_disable_mayhem_for_tutorial calls Mayhem_activity.disable directly. Confirmed by gameflow.md:589. |
| `gameflow.md:321` | render_inactive_icons read All_activities[ACTIVITY_MODE_HOUSE_ARREST] and treated a null slot as not starting; it reads House_arrest_activity again | The null-slot stand-in was removed (gameflow.md:437). | activity_raid_attack.cpp:515 reads House_arrest_activity directly; no All_activities[HOUSE_ARREST] read found. |
| `gameflow.md:321` | collect_layer_objects<T> was defined in activity_raid.cpp; it is now a template of activity_base in activity_internal.h | It moved when a second file needed it, as the doc planned. | Now shared by activity.cpp, activity_raid.cpp and activity_house_arrest.cpp. |
| `gameflow.md:437` | render_inactive_icons read All_activities and activity_system_level_init_post started house arrest through it; they read House_arrest_activity and call house_arrest_level_init_post | The doc says these earlier deviations were removed. | activity_raid_attack.cpp reads House_arrest_activity directly. Not found to still deviate. |
| `lod.md:298` | The test-side save/restore of the global RNG around GPU visibility lived in viewer.cpp; the viewer is deleted and rl_gpu_visibility.cpp has no such seam. | It hid an original RNG hazard from the viewer's tests. | tests/viewer was deleted on purpose; no RNG save/restore found in rl_gpu_visibility.cpp or tests. |
| `memory/mp-aim-parallax.md:18` | Client sends the camera forward vector instead of head-to-aim direction when their dot product is below cos 45 degrees. | Client aim bent on the server near other players. | multi_game_client_predict.cpp packs aim_pos minus head only; no cos 45 fallback in the code (the memory note says it was not run). |
| `memory/om7-implementation.md:19` | Quit Game in the Esc menu skipped the MENU_QUIT_CONFIRM_MP yes/no box. | Placeholder shortcut (2026-10-26 user request). | Original box with MENU_QUIT_CONFIRM_MP / MENU_QUIT_CONFIRM / wrecking crew text is shown. |
| `multiplayer.md:155` | A --connect IP[:port] option that fills one search result (lobby id = network address) for machines where UDP broadcast to self does not arrive. | Broadcast to self may not arrive on some machines. | No --connect in src or tests; the test programs that had it were deleted. OpenRFG has --ip/--port direct join and --search instead. |
| `multiplayer.md:311` | The handshake makes no deferred camera when there is no main scene. | Headless programs have no render camera. | gamestate_gp_handshake and game_render_setup_frame have no null guard or deferred camera code; nothing found. |
| `multiplayer.md:459` | Replay test feeds two clients the same server packet stream (--clients 2) instead of recording and replaying packets in one process. | A single-process replay needs a level load outside the lobby. | The multiplayer test programs were deleted; no --clients option in tests. |
| `multiplayer.md:628` | Wrecking Crew per-character backpack offsets are not read (doc claim). | Wrecking Crew was single player DLC outside the multiplayer scope. | Code now reads Wrecking_crew_characters[...]->backpack_offsets when wrecking_crew_is_active (around line 1899). |
| `orig_main.md:446` | Framework input test used posted window messages and subsequence ordering, with Alt+Enter checked by swap chain resize. | SendInput needs the foreground window. | No PostMessage or toggleWindowMode in tests; the om1-om4 tests were deleted (tests-removed). |
| `orig_main.md:643` | hud_ui_is_loaded stays false in OpenRFG (doc claim). | Its caller drew with HUD vint bitmaps that were not loaded. | Now the original expression: Cant_load \|\| (!Unloaded && Data_items_created_uihud); the HUD is ported and sets the flag. |
| `orig_mvp.md:146` | OpenRFG --screenshot FRAME FILE option through the g_openrfgRenderFrameDone seam. | Test seam for capturing frames. | No --screenshot option in src/openrfg/main_options.cpp. The g_openrfgRenderFrameDone seam itself remains in application.cpp. |
| `orig_mvp.md:146` | OpenRFG --render-profile minimal switch applying the viewer's minimal profile. | For bisecting a broken pass. | No render-profile option in the code. |
| `orig_mvp.md:146` | The RNG save/restore seam of lod.md used by the viewers. | Kept the global RNG stream equal between viewer runs. | No RNG save/restore seam found in src; viewers were deleted. |
| `orig_mvp.md:5` | viewer_mp's minimal render profile (viewer_apply_minimal_switches) set by the test driver. | Was a test-driver profile for the viewer. | No match for viewer_apply_minimal_switches or minimal render profile anywhere in the repo; viewers were deleted. |
| `port_history/src_status.md:263` | GameLinkInternet was an offline stand-in in mp_stubs.cpp. | Inferred: internet matchmaking was not needed for LAN deathmatch. | src/stubs mp_stubs.cpp is gone; src/library/modules/runtime/gamelink/sources/steam/gamelinkinternet_steam.cpp holds real GameLinkInternet code. |
| `port_history/src_status.md:275` | The language selection in localization_shared.cpp stayed a stub. | Inferred: not needed for the UI foundation at that stage. | localization_shared.cpp now uses keen::findSupportedLanguage for the system language. |
| `stub_impl.md:372` | The version patch manager singleton is null until SG, with applyPatches and getUid as stand-ins. | Patches were not ported at the time. | hkVersionPatchManagercreate registers the singleton (line 25-32), getUid and applyPatches have bodies. |
| `stub_impl.md:525` | hud_ui_mission_briefing_get_line_info was a stand-in returning null until S11. | S11 had not ported the mission briefing table. | Now a real function; src/stubs has no entries for it, wrecking_crew_init, camera_satellite_* or Activity_set_mission_caption. |
| `stub_impl.md:525` | wrecking_crew_init/_shutdown, camera_satellite_* and Activity_set_mission_caption were generated stand-ins until S10/S11. | Those subsystems were not yet ported. | No stand-ins for these names in src/stubs. |
| `stub_misc.md:196` | turret_physics_set_control_type is one shared helper where rfg.exe inlines the logic into callers. | Inferred: one body instead of repeated inline copies. | Not a deviation: rfg.exe has this function at 0x00A21940 with the same three callers as the port. |
| `stub_misc.md:305` | Int3 prints in cheats_menu.cpp include __FILE__ and __LINE__, which rfg.exe's stripped strings do not show. | Inferred: gives the debug print a location the original lacks. | Not a deviation: rfg.exe pushes the file and line for this print too (0x0088F53D). |
| `wrecking.md:43` | Per-character backpack offsets were not read (MP10 deviation). | Was an MP10 simplification. | Offsets for the current Wrecking Crew player are read now. |

## Not settled

The check could not decide these.

| Doc | Deviation | Why it was made | What was found |
|---|---|---|---|
| `gameflow.md:374` | Plain float where rfg.exe uses double chains and the library's sin, cos and tan for x87 ones in the cutscene code | Inferred: not bit-exact, accepted. | 13 functions sampled against rfg.exe; the 6 that differed now match. Not sampled: the other cutscene functions and camera_satellite.cpp's x87 calls. |
| `memory/gf4-implementation.md:15` | is_on_restricted_list in safehouse_vip.cpp goes through getters (get_human_info, activity_vrail_is_riding_shotgun_running). | Inferred: statics of other TUs are reached through getters. | No comment states a deviation; could not tell which static the getter replaces. |
| `stub_impl.md:389` | The hkSerializeDeprecated request of initialize stays skipped | Belongs to serializer part 2, which is deferred | deferred.md still lists SG part 2, but the request site was not located in the code. |
| `stub_misc.md:350` | do_camera_movement merges the asm's repeated paths for the user's turn rate into one. | Inferred: the doc says it should be the same logic. | Merged turn-rate paths not found without the asm; no comment says so. |
| `stub_misc.md:350` | A human target is cast directly, where rfg.exe looks its own handle up again. | Inferred: the cast avoids a second handle lookup. | Direct (human*) casts exist at camera_free.cpp lines 399, 401, 839 but which one the doc means was not settled. |

## Other deviations commented in the code

These come from code comments and appear in no doc, so they were **not checked individually**. The text is the comment's own.

### Havok SDK

| Location | Comment |
|---|---|
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/baseobjecthkreferencedobjectreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkReferencedObject::Members (hkReferencedObject_Members here: the header of hkReferencedObject in src does not spell out Members[]) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/hkaabb16reflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkAabb16TypeInfo (hkAabb16 not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/hkaabb20reflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkAabb24_16_24TypeInfo (hkAabb24_16_24 not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/hkaabbhalfreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkAabbHalfTypeInfo (hkAabbHalf not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/hkbasetypesreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkFloat16::Members (hkFloat16_Members here: the header of hkFloat16 in src does not spell out Members[]) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/hkbitfieldreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkBitFieldBase<hkBitFieldStorage<hkArray<unsigned int,hkContainerHeapAllocator> > >::Members ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/hkbitfieldreflection.cpp:12` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkBitFieldTypeInfo (hkBitField not declared in src yet) - hkBitFieldBasehkBitFieldStoragehkArrayunsignedinthkContainerHeapAllocatorTypeInfo ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/hkcolorreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkColorTypeInfo (hkColor not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/hkfloat16transformreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkFloat16Transform::Members (hkFloat16Transform_Members here: no declaration of hkFloat16Transform in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/hkfloat16transformreflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkFloat16TransformTypeInfo (hkFloat16Transform not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/hklocalframereflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkLocalFrameGroup::Members (hkLocalFrameGroup_Members here: no declaration of hkLocalFrameGroup in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/hklocalframereflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkLocalFrameGroupTypeInfo (hkLocalFrameGroup not declared in src yet) - hkSimpleLocalFrameTypeInfo (hkSimpleLocalFrame not declared in src ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/hkmonitorstreamhkmonitorstreamanalyzerreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkMonitorStreamColorTableColorPairTypeInfo (hkMonitorStreamColorTable::ColorPair not declared in src yet) - hkMonitorStreamColorTableTypeInfo ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/hkmotionstatereflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkMotionState::Members (hkMotionState_Members here: the header of hkMotionState in src does not spell out Members[]) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/hkpackedvector3reflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkPackedVector3TypeInfo (hkPackedVector3 not declared in src yet) - hkPackedVector8_3TypeInfo (hkPackedVector8_3 not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/hkqtransformreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkQTransformTypeInfo (hkQTransform not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/hkrefcountedpropertiesreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkRefCountedProperties::Members (hkRefCountedProperties_Members here: no declaration of hkRefCountedProperties in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/hkrefcountedpropertiesreflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkRefCountedPropertiesEntryTypeInfo (hkRefCountedProperties::Entry not declared in src yet) - hkRefCountedPropertiesTypeInfo ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/hksetreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkSet<hkIntRealPair,hkContainerHeapAllocator,hkMapOperations<hkIntRealPair> >::Members ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/hksetreflection.cpp:13` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkIntRealPairTypeInfo (hkIntRealPair not declared in src yet) - hkSetIntFloatPairTypeInfo (hkSetIntFloatPair not declared in src yet) - ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/hkspherereflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkSphere::Members (hkSphere_Members here: the header of hkSphere in src does not spell out Members[]) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/hkstringobjectreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkStringObjectTypeInfo (hkStringObject not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/hktracestreamreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkTraceStreamTitleTypeInfo (hkTraceStream::Title not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/hkuuidreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkUuid::Members (hkUuid_Members here: the header of hkUuid in src does not spell out Members[]) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/classes/reflections/hkuuidreflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkUuidObjectTypeInfo (hkUuidObject not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassversion1class.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkClassVersion1::Members (hkClassVersion1_Members here: no declaration of hkClassVersion1 in src yet) - hkClassVersion2::Members ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassversion1paddedclass.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkClassVersion1Padded::Members (hkClassVersion1Padded_Members here: no declaration of hkClassVersion1Padded in src yet) - ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/classes/reflections/hkcdplanarcsgoperandreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkcdPlanarCsgOperand::Members (hkcdPlanarCsgOperand_Members here: no declaration of hkcdPlanarCsgOperand in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/classes/reflections/hkcdplanarcsgoperandreflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkcdPlanarCsgOperandTypeInfo (hkcdPlanarCsgOperand not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/classes/reflections/hkcdplanarentityreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkcdPlanarEntity::Members (hkcdPlanarEntity_Members here: no declaration of hkcdPlanarEntity in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/classes/reflections/hkcdplanarentityreflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkcdPlanarEntityTypeInfo (hkcdPlanarEntity not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/classes/reflections/hkcdplanargeometryplanescollectionreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkcdPlanarGeometryPlanesCollection::Members (hkcdPlanarGeometryPlanesCollection_Members here: no declaration of ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/classes/reflections/hkcdplanargeometryplanescollectionreflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkcdPlanarGeometryPlanesCollectionTypeInfo (hkcdPlanarGeometryPlanesCollection not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/classes/reflections/hkcdplanargeometrypolygoncollectionreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkcdPlanarGeometryPolygonCollection::Material::Members (hkcdPlanarGeometryPolygonCollection_Material_Members here: no declaration of ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/classes/reflections/hkcdplanargeometrypolygoncollectionreflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkcdPlanarGeometryPolygonCollectionMaterialTypeInfo (hkcdPlanarGeometryPolygonCollection::Material not declared in src yet) - ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/classes/reflections/hkcdplanargeometryprimitivescollectionreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkcdPlanarGeometryPrimitives::Collection<28>::Members (hkcdPlanarGeometryPrimitives_Collection_28_Members here: no declaration of ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/classes/reflections/hkcdplanargeometryprimitivescollectionreflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkcdPlanarGeometryPrimitivesCollection28TypeInfo (hkcdPlanarGeometryPrimitivesCollection28_typename not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/classes/reflections/hkcdplanargeometryprimitivesreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkcdPlanarGeometryPrimitives::Plane::Members (hkcdPlanarGeometryPrimitives_Plane_Members here: no declaration of ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/classes/reflections/hkcdplanargeometryprimitivesreflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkcdPlanarGeometryPrimitivesPlaneTypeInfo (hkcdPlanarGeometryPrimitives::Plane not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/classes/reflections/hkcdplanargeometryreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkcdPlanarGeometry::Members (hkcdPlanarGeometry_Members here: no declaration of hkcdPlanarGeometry in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/classes/reflections/hkcdplanargeometryreflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkcdPlanarGeometryTypeInfo (hkcdPlanarGeometry not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/classes/reflections/hkcdplanarsolidreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkcdPlanarSolid::NodeStorage::Members (hkcdPlanarSolid_NodeStorage_Members here: no declaration of hkcdPlanarSolid::NodeStorage in src ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/classes/reflections/hkcdplanarsolidreflection.cpp:12` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkcdPlanarSolidNodeStorageTypeInfo (hkcdPlanarSolid::NodeStorage not declared in src yet) - hkcdPlanarSolidNodeTypeInfo ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/classes/reflections/internalhkcdstaticmeshtreereflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkcdStaticMeshTreeBase::Section::SharedVertices::Members (hkcdStaticMeshTreeBase_Section_SharedVertices_Members here: no declaration of ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/classes/reflections/internalhkcdstaticmeshtreereflection.cpp:14` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkcdStaticMeshTreeBaseSectionSharedVerticesTypeInfo (hkcdStaticMeshTreeBase::Section::SharedVertices not declared in src yet) - ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/classes/reflections/internalhkcdstatictreecodecsreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkcdStaticTreeCodec3AxisTypeInfo (hkcdStaticTree::Codec3Axis not declared in src yet) - hkcdStaticTreeCodec3Axis4TypeInfo ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/classes/reflections/internalhkcdstatictreereflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkcdStaticTreeDefaultTreeStorage32TypeInfo (hkcdStaticTree::DefaultTreeStorage32 not declared in src yet) - ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/classes/reflections/internalhkcdstatictreestoragesreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkcdStaticTreeDynamicStorage32TypeInfo (hkcdStaticTree::DynamicStorage32 not declared in src yet) - hkcdStaticTreeDynamicStorage4TypeInfo ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/classes/reflections/hkpconstraintatomreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpBallSocketConstraintAtom::Members (hkpBallSocketConstraintAtom_Members here: the header of hkpBallSocketConstraintAtom in src does ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/classes/reflections/hkpconstraintatomreflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkp3dAngConstraintAtomTypeInfo (hkp3dAngConstraintAtom not declared in src yet) - hkpCogWheelConstraintAtomTypeInfo ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/classes/reflections/hkpwrappedconstraintdatareflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpWrappedConstraintData::Members (hkpWrappedConstraintData_Members here: the header of hkpWrappedConstraintData in src does not spell ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/bvtreestream/hkpbvtreestreamagent.cpp:7` | Deviation: the stream agent itself (an agent2 over a 1n agent track) is not ported: its create functions make the BV tree agent2 instead. It is live in rfg.exe on the character controller's capsule phantom, not tied to the multithreaded paths; its queries only forward to the hkpBvTreeAgent statics, and no reachable caller of its differing processCollision was found (inferred, not run). |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpcollisionfilterlistreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpCollisionFilterList::Members (hkpCollisionFilterList_Members here: the header of hkpCollisionFilterList in src does not spell out ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpcollisionfilterlistreflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpCollisionFilterListTypeInfo (the finishing constructor of hkpCollisionFilterList is not ported yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpcompressedmeshshapehkpcompressedmeshshapereflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpCompressedMeshShapeChunkTypeInfo (hkpCompressedMeshShape::Chunk not declared in src yet) - hkpCompressedMeshShapeBigTriangleTypeInfo ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpconvexlistshapereflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpConvexListShape::Members (hkpConvexListShape_Members here: the header of hkpConvexListShape in src does not spell out Members[]) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpconvexlistshapereflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpConvexListShapeTypeInfo (the finishing constructor of hkpConvexListShape is not ported yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpconvexpiecemeshshapehkpconvexpiecemeshshapereflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpConvexPieceMeshShape::Members (hkpConvexPieceMeshShape_Members here: no declaration of hkpConvexPieceMeshShape in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpconvexpiecemeshshapehkpconvexpiecemeshshapereflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpConvexPieceMeshShapeTypeInfo (hkpConvexPieceMeshShape not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpconvexverticesconnectivityreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpConvexVerticesConnectivityTypeInfo (the finishing constructor of hkpConvexVerticesConnectivity is not ported yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpextendedmeshshapehkpstorageextendedmeshshapereflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpStorageExtendedMeshShape::Members (hkpStorageExtendedMeshShape_Members here: no declaration of hkpStorageExtendedMeshShape in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpextendedmeshshapehkpstorageextendedmeshshapereflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpStorageExtendedMeshShapeMaterialTypeInfo (hkpStorageExtendedMeshShape::Material not declared in src yet) - ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpgroupfilterreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpGroupFilterTypeInfo (the finishing constructor of hkpGroupFilter is not ported yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpheightfieldhkpcompressedsampledheightfieldshapereflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpCompressedSampledHeightFieldShapeTypeInfo (hkpCompressedSampledHeightFieldShape not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpheightfieldhkpplaneshapereflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpPlaneShape::Members (hkpPlaneShape_Members here: no declaration of hkpPlaneShape in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpheightfieldhkpplaneshapereflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpPlaneShapeTypeInfo (hkpPlaneShape not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpmeshshapehkpfastmeshshapereflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpFastMeshShapeTypeInfo (hkpFastMeshShape not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpmeshshapehkpmeshshapereflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpMeshShape::Members (hkpMeshShape_Members here: no declaration of hkpMeshShape in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpmeshshapehkpmeshshapereflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpMeshShapeSubpartTypeInfo (hkpMeshShape::Subpart not declared in src yet) - hkpMeshShapeTypeInfo (hkpMeshShape not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpmeshshapehkpstoragemeshshapereflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpStorageMeshShape::Members (hkpStorageMeshShape_Members here: no declaration of hkpStorageMeshShape in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpmeshshapehkpstoragemeshshapereflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpStorageMeshShapeSubpartStorageTypeInfo (hkpStorageMeshShape::SubpartStorage not declared in src yet) - hkpStorageMeshShapeTypeInfo ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpmultirayshapereflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpMultiRayShape::Members (hkpMultiRayShape_Members here: no declaration of hkpMultiRayShape in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpmultirayshapereflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpMultiRayShapeRayTypeInfo (hkpMultiRayShape::Ray not declared in src yet) - hkpMultiRayShapeTypeInfo (hkpMultiRayShape not declared in src ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpmultisphereshapereflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpMultiSphereShapeTypeInfo (the finishing constructor of hkpMultiSphereShape is not ported yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpnamedmeshmaterialreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpNamedMeshMaterialTypeInfo (hkpNamedMeshMaterial not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpremoveterminalsmoppmodifierreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpRemoveTerminalsMoppModifier::Members (hkpRemoveTerminalsMoppModifier_Members here: no declaration of hkpRemoveTerminalsMoppModifier ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpremoveterminalsmoppmodifierreflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpRemoveTerminalsMoppModifierTypeInfo (hkpRemoveTerminalsMoppModifier not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpshapeinforeflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpShapeInfoTypeInfo (hkpShapeInfo not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkpsimplemeshshapehkpsimplemeshshapereflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpSimpleMeshShapeTriangleTypeInfo (hkpSimpleMeshShape::Triangle not declared in src yet) - hkpSimpleMeshShapeTypeInfo (hkpSimpleMeshShape ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/classes/reflections/hkptransformshapereflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpTransformShapeTypeInfo (the finishing constructor of hkpTransformShape is not ported yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkparrayactionreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpArrayAction::Members (hkpArrayAction_Members here: no declaration of hkpArrayAction in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpballsocketchaindatareflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpBallSocketChainData::Members (hkpBallSocketChainData_Members here: no declaration of hkpBallSocketChainData in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpballsocketchaindatareflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpBallSocketChainDataConstraintInfoTypeInfo (hkpBallSocketChainData::ConstraintInfo not declared in src yet) - ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpbinaryactionreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpBinaryAction::Members (hkpBinaryAction_Members here: no declaration of hkpBinaryAction in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpbreakablebodyreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpBreakableBody::Controller::Members (hkpBreakableBody_Controller_Members here: no declaration of hkpBreakableBody::Controller in src ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpbreakablebodyreflection.cpp:12` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpBreakableBodyControllerTypeInfo (hkpBreakableBody::Controller not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpbreakablematerialreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpBreakableMaterial::Members (hkpBreakableMaterial_Members here: no declaration of hkpBreakableMaterial in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpbreakablemultimaterialreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpBreakableMultiMaterial::Members (hkpBreakableMultiMaterial_Members here: no declaration of hkpBreakableMultiMaterial in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpbreakablemultimaterialreflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpBreakableMultiMaterialInverseMappingDescriptorTypeInfo (hkpBreakableMultiMaterial::InverseMappingDescriptor not declared in src yet) - ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpbreakableshapereflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpBreakableShape::Members (hkpBreakableShape_Members here: no declaration of hkpBreakableShape in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpbreakableshapereflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpBreakableShapeTypeInfo (hkpBreakableShape not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpconstraintchaininstanceactionreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpConstraintChainInstanceActionTypeInfo (hkpConstraintChainInstanceAction not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpconstraintchaininstancereflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpConstraintChainInstanceTypeInfo (hkpConstraintChainInstance not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpdefaultworldmemorywatchdogreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpDefaultWorldMemoryWatchDog::Members (hkpDefaultWorldMemoryWatchDog_Members here: no declaration of hkpDefaultWorldMemoryWatchDog in ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpdefaultworldmemorywatchdogreflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpDefaultWorldMemoryWatchDogTypeInfo (hkpDefaultWorldMemoryWatchDog not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpemsbreakablematerialreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpExtendedMeshShapeBreakableMaterialTypeInfo (hkpExtendedMeshShapeBreakableMaterial not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpgenericconstraintdatareflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpGenericConstraintData::Members (hkpGenericConstraintData_Members here: no declaration of hkpGenericConstraintData in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpgenericconstraintdatareflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpGenericConstraintDataTypeInfo (hkpGenericConstraintData not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpgenericconstraintschemereflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpGenericConstraintDataSchemeConstraintInfoTypeInfo (hkpGenericConstraintDataScheme::ConstraintInfo not declared in src yet) - ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkplistshapebreakablematerialreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpListShapeBreakableMaterialTypeInfo (hkpListShapeBreakableMaterial not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpmalleableconstraintdatareflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpMalleableConstraintDataTypeInfo (hkpMalleableConstraintData not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpmaterialreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpMaterial::Members (hkpMaterial_Members here: the header of hkpMaterial in src does not spell out Members[]) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpmodifierconstraintatomreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpViscousSurfaceModifierConstraintAtomTypeInfo (hkpViscousSurfaceModifierConstraintAtom not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpphysicssystemreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpPhysicsSystem::Members (hkpPhysicsSystem_Members here: no declaration of hkpPhysicsSystem in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpphysicssystemreflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpPhysicsSystemTypeInfo (hkpPhysicsSystem not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkppoweredchaindatahkppoweredchaindatareflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpPoweredChainDataConstraintInfoTypeInfo (hkpPoweredChainData::ConstraintInfo not declared in src yet) - hkpPoweredChainDataTypeInfo ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpscsbreakablematerialreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpStaticCompoundShapeBreakableMaterialTypeInfo (hkpStaticCompoundShapeBreakableMaterial not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpsimplebreakablematerialreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpSimpleBreakableMaterialTypeInfo (hkpSimpleBreakableMaterial not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/classes/reflections/hkpstiffspringchaindatareflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpStiffSpringChainDataConstraintInfoTypeInfo (hkpStiffSpringChainData::ConstraintInfo not declared in src yet) - ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/classes/reflections/hkpbvcompressedmeshshapereflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpBvCompressedMeshShape::Members (hkpBvCompressedMeshShape_Members here: the header of hkpBvCompressedMeshShape in src does not spell ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/classes/reflections/hkpbvcompressedmeshshapereflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpBvCompressedMeshShapeTypeInfo (the finishing constructor of hkpBvCompressedMeshShape is not ported yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/classes/reflections/hkpconvexpiecestreamdatareflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpConvexPieceStreamDataTypeInfo (hkpConvexPieceStreamData not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/classes/reflections/hkpmoppcodereflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpMoppCodeReindexedTerminalTypeInfo (hkpMoppCodeReindexedTerminal not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/classes/reflections/hkpshapekeytablereflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpShapeKeyTableBlockTypeInfo (the finishing constructor of hkpShapeKeyTable::Block is not ported yet) - hkpShapeKeyTableTypeInfo (the ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/classes/reflections/hkpstaticcompoundshapereflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpStaticCompoundShapeTypeInfo (the finishing constructor of hkpStaticCompoundShape is not ported yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/classes/reflections/internalhkcdstaticmeshtreereflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkcdStaticMeshTreeBase::PrimitiveDataRunBase<unsigned int>::Members (hkcdStaticMeshTreeBase_PrimitiveDataRunBase_unsigned_int_Members ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/classes/reflections/internalhkcdstaticmeshtreereflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkcdStaticMeshTreeBasePrimitiveDataRunBaseunsignedintTypeInfo (hkcdStaticMeshTreeBasePrimitiveDataRunBaseunsignedint_typename not declared in ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/classes/reflections/internalhkpbvcompressedmeshshapeinternalsreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpBvCompressedMeshShapeTreeTypeInfo (hkpBvCompressedMeshShapeTree not declared in src yet) - hkpBvCompressedMeshShapeTreeDataRunTypeInfo ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpangulardashpotactionreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpAngularDashpotActionTypeInfo (hkpAngularDashpotAction not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpballgunreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpBallGunTypeInfo (hkpBallGun not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpcharacterrigidbodycinforeflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpCharacterRigidBodyCinfoTypeInfo (hkpCharacterRigidBodyCinfo not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpconstrainedsystemfilterreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpConstrainedSystemFilter::Members (hkpConstrainedSystemFilter_Members here: no declaration of hkpConstrainedSystemFilter in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpconstrainedsystemfilterreflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpConstrainedSystemFilterTypeInfo (hkpConstrainedSystemFilter not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpdashpotactionreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpDashpotAction::Members (hkpDashpotAction_Members here: no declaration of hkpDashpotAction in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpdashpotactionreflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpDashpotActionTypeInfo (hkpDashpotAction not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpdisableentitycollisionfilterreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpDisableEntityCollisionFilterTypeInfo (hkpDisableEntityCollisionFilter not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpdisplaybindingdatareflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpDisplayBindingDataRigidBodyTypeInfo (hkpDisplayBindingData::RigidBody not declared in src yet) - ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpfirstpersongunreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpFirstPersonGunTypeInfo (hkpFirstPersonGun not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpgravitygunreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpGravityGunTypeInfo (hkpGravityGun not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpgroupcollisionfilterreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpGroupCollisionFilterTypeInfo (hkpGroupCollisionFilter not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpmotoractionreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpMotorAction::Members (hkpMotorAction_Members here: no declaration of hkpMotorAction in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpmotoractionreflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpMotorActionTypeInfo (hkpMotorAction not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpmountedballgunreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpMountedBallGunTypeInfo (hkpMountedBallGun not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpmousespringactionreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpMouseSpringActionTypeInfo (hkpMouseSpringAction not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpphysicsdatareflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpPhysicsData::Members (hkpPhysicsData_Members here: no declaration of hkpPhysicsData in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpphysicsdatareflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpPhysicsDataTypeInfo (hkpPhysicsData not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpphysicssystemwithcontactsreflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpPhysicsSystemWithContacts::Members (hkpPhysicsSystemWithContacts_Members here: no declaration of hkpPhysicsSystemWithContacts in src ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpphysicssystemwithcontactsreflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpPhysicsSystemWithContactsTypeInfo (hkpPhysicsSystemWithContacts not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkppoweredchainmapperreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpPoweredChainMapperTargetTypeInfo (hkpPoweredChainMapper::Target not declared in src yet) - hkpPoweredChainMapperLinkInfoTypeInfo ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpprojectilegunreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpProjectileGunTypeInfo (hkpProjectileGun not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpreorientactionreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpReorientActionTypeInfo (hkpReorientAction not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpserializedagentnnentryreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpSerializedAgentNnEntryTypeInfo (hkpSerializedAgentNnEntry not declared in src yet) - hkpSerializedSubTrack1nInfoTypeInfo ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpserializeddisplaymarkerlistreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpSerializedDisplayMarkerListTypeInfo (hkpSerializedDisplayMarkerList not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpserializeddisplaymarkerreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpSerializedDisplayMarkerTypeInfo (hkpSerializedDisplayMarker not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpserializeddisplayrbtransformsreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpSerializedDisplayRbTransformsDisplayTransformPairTypeInfo (hkpSerializedDisplayRbTransforms::DisplayTransformPair not declared in src yet) ... |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkpspringactionreflection.cpp:8` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpSpringActionTypeInfo (hkpSpringAction not declared in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkptriggervolumereflection.cpp:8` | Deviation: static Members arrays defined as file statics (the class in src does not spell them out): - hkpTriggerVolume::Members (hkpTriggerVolume_Members here: no declaration of hkpTriggerVolume in src yet) |
| `src/volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/classes/reflections/hkptriggervolumereflection.cpp:11` | Deviation: type infos not generated (the class or its finishing constructor is not in src yet): - hkpTriggerVolumeEventInfoTypeInfo (hkpTriggerVolume::EventInfo not declared in src yet) - hkpTriggerVolumeTypeInfo (hkpTriggerVolume not ... |

### audio_lib

| Location | Comment |
|---|---|
| `src/volition/audio_lib/code/audiolib.cpp:1556` | Deviation (SFX.md 3.1): the --audio-log test seam, off (null) by default. The retail data has no cue names, so the log learns them from the lookups by name. |
| `src/volition/audio_lib/code/audiolib_api.h:207` | Deviation (SFX.md 3.1, not in rfg.pdb): the --audio-log test seam, null unless a program sets them. The lookups by name that succeed (the name and its caller cue id), and every audiolib_cue_play result (play id or -1, the error). |

### game

| Location | Comment |
|---|---|
| `src/game/sources/application.cpp:503` | Deviation 2 (orig_main.md section 3, off by default): -gamebuild DIR mounts DIR as /gamebuild instead of <exe dir>/data, so the program runs outside the game folder (the tests' data directory). |
| `src/game/sources/application.cpp:517` | Deviation 3: --window-pos gives physical pixels, but this process is not DPI aware (as rfg.exe), so Windows scales its window coordinates by the monitor's DPI. The monitor under the point has one rectangle per awareness, which gives the mapping (a monitor's ... |
| `src/game/sources/application.cpp:728` | Deviation 4 (orig_main.md section 3): the backtrace upload never sends anything (backtraceuploader.cpp). |
| `src/game/sources/application.hpp:109` | Port deviation 1 (orig_main.md 2.5, stub_impl.md S2): GameFrameworkParameters::useSteam, which rfg.exe always sets. true (the default) is rfg.exe's Steam start-up, for which OpenRFG sets the Steam app id environment; --no-steam clears it. |
| `src/game/sources/application.hpp:113` | Port deviation 4: where the save data goes. rfg.exe always uses Steam's remote storage, OpenRFG too by default. The local SaveDataProviderWin32Native (the same file format) in g_openrfgSaveDirectory (--save-dir; null: the default folder) is used instead with --no-steam, --no-steam-cloud-saves or --save-dir; the user's ... |
| `src/game/sources/application.hpp:119` | Port deviation 3 (orig_main.md section 3): OpenRFG's --window-pos X Y, the top left corner of the window's frame in physical desktop pixels. Off (rfg.exe's fixed position) unless g_openrfgWindowPosSet. |

### havok

| Location | Comment |
|---|---|
| `src/volition/libmodules/havok/hk2014_1_0_r1/Source/Common/Base/UnitTest/hkUnitTest.h:251` | / Return the next single precision value as Gaussian deviate (ie. with mean 0 and standard deviation 1) |
| `src/volition/libmodules/havok/hk2014_1_0_r1/Source/Common/Base/UnitTest/hkUnitTest.h:254` | / Return the next double precision value as Gaussian deviate (ie. with mean 0 and standard deviation 1) |
| `src/volition/libmodules/havok/hk2014_1_0_r1/Source/Common/Base/UnitTest/hkUnitTest.h:257` | / Return the next hkReal value as Gaussian deviate (ie. with mean 0 and standard deviation 1) |

### keen/base

| Location | Comment |
|---|---|
| `src/library/modules/runtime/base/sources/win32/memorylayout_win32.cpp:128` | OpenRFG deviation: the message box is replaced by a log line. MessageBoxA(nullptr, message, "Error", MB_ICONHAND); |

### keen/gameframework

| Location | Comment |
|---|---|
| `src/library/modules/runtime/gameframework/sources/global/gameframework.cpp:7` | Deviation (orig_main.md 3.1, stub_impl.md S2): Application::fillGameframeworkParameters sets useSteam = false when OpenRFG runs with --no-steam; initialize then creates no Steam services (Steamworks, user accounts, save data, system services). The deviations ... |
| `src/library/modules/runtime/gameframework/sources/global/gameframework.cpp:444` | OpenRFG deviation: the message box is replaced by a log line. MessageBoxW(nullptr, message, L"Steamworks", 0); |
| `src/library/modules/runtime/gameframework/sources/global/gameframework.cpp:451` | OpenRFG deviation: the message box is replaced by a log line. MessageBoxW(nullptr, message, L"Steamworks", 0); |
| `src/library/modules/runtime/gameframework/sources/global/gameframework.cpp:488` | OpenRFG deviation: the message box is replaced by a log line. MessageBoxW(nullptr, text, caption, 0); |
| `src/library/modules/runtime/gameframework/sources/global/gameframework.cpp:517` | Deviation (application.cpp deviation 4): rfg.exe only has Steam's save data system. The application chooses the local one on request, so the failure check below is for Steam's alone. |
| `src/library/modules/runtime/gameframework/sources/global/gameframework.cpp:537` | OpenRFG deviation: the message box is replaced by a log line. MessageBoxW(nullptr, text, L"Steamworks", 0); |
| `src/library/modules/runtime/gameframework/sources/win32/backtraceuploader.cpp:6` | Deviation (orig_main.md 3.4): a reimplementation never uploads anything; uploadToBacktrace does nothing (the minidump stays next to the exe, the log next to it). |
| `src/library/modules/runtime/gameframework/sources/win32/gameframework_win32.cpp:7` | Deviation: the framework's message boxes are commented out and replaced by log lines, which carry the string table text of gameframeworklocatexts.rc (gameframework.cpp's loadFrameworkMessage, handleFatalError). |
| `src/library/modules/runtime/gameframework/sources/win32/gameframework_win32.cpp:298` | Asks whether to write a crash report; yes writes the minidump, closes the log and uploads both (deviation, orig_main.md 3.4: the upload sends nothing). |
| `src/library/modules/runtime/gameframework/sources/win32/gameframework_win32.cpp:304` | other messages). Deviation: the buffer is emptied first (see gameframework.cpp's loadFrameworkMessage). |
| `src/library/modules/runtime/gameframework/sources/win32/gameframework_win32.cpp:327` | OpenRFG deviation: the message box is replaced by a log line. Magic number: MB_ICONERROR \| MB_SYSTEMMODAL \| MB_SETFOREGROUND. MessageBoxW(nullptr, L"Unable to write diagnostic file.", L"The Game Crashed!", 0x12010); |

### keen/gamelink

| Location | Comment |
|---|---|
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp:15` | other as shipped; see the localhost deviation below (multiplayer.md 2.3). |
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp:29` | the own address deviation (openrfgGetLanOwnHostAddress) |
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp:228` | instance on the same machine fails here (the port is taken) unless the localhost deviation gives it a port. |
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp:606` | deviation: see openrfgGetLanOwnHostAddress |
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp:1008` | deviation: see openrfgGetLanOwnHostAddress |
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp:1057` | this instance's station: the host address on the local port (deviation; original: the |
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp:1058` | matchmaking port, which is the same when the deviation is off) |
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp:1060` | deviation: see openrfgGetLanOwnHostAddress |
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.hpp:253` | OpenRFG deviation (multiplayer.md 2.3, new code; see gamelinklan.cpp): the local LAN port, 0 = original behaviour. Set before beginSession (the multiplayer programs' --lan-port). |
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.hpp:257` | OpenRFG deviation (part of the one above; see gamelinklan.cpp): the host's own address, the default route's |
| `src/library/modules/runtime/gamelink/sources/global/lan/gamelinklan.hpp:258` | source address when the deviation is on (original: the local host's first address). The port is left as it is. False without a local address. |
| `src/library/modules/runtime/gamelink/sources/steam/gamelinkinternet_steam.cpp:15` | No-Steam deviation (stub_impl.md S2, OpenRFG with --no-steam): sw_api.dll has no interfaces before SteamAPI_Init, so create() skips the voice sample rate query and beginSession() fails; the link then stays offline and makes no other Steam call (the ... |
| `src/library/modules/runtime/gamelink/sources/steam/gamelinkinternet_steam.cpp:185` | deviation: no Steam (no voice sample rate; the voice contexts are never created) |

### keen/gamesession

| Location | Comment |
|---|---|
| `src/library/modules/runtime/gamesession/sources/global/localgamesession.cpp:11` | Deviation (OpenRFG with --no-steam; application.cpp deviation 4): rfg.exe always has a user account system. Without one the player has no account, and goes straight to the save data as an anonymous user: never signed in, so no achievements or online ... |
| `src/library/modules/runtime/gamesession/sources/global/localgamesession.cpp:493` | deviation: no account to lose without Steam |
| `src/library/modules/runtime/gamesession/sources/global/localgamesession.cpp:562` | deviation: without Steam, on to the save data |

### keen/graphics

| Location | Comment |
|---|---|
| `src/library/modules/runtime/graphics/sources/d3d11/graphicssystem_d3d11.cpp:2261` | OpenRFG deviation: the message box is replaced by a log line. MessageBoxA(nullptr, message, "Fatal error", MB_OK); |

### keen/savedata

| Location | Comment |
|---|---|
| `src/library/modules/runtime/savedata/sources/global/savedatahandler.cpp:8` | Deviation: rfg.exe always has a save data system. OpenRFG has one too (the local one by default, application.cpp |
| `src/library/modules/runtime/savedata/sources/global/savedatahandler.cpp:9` | deviation 4); only the test programs that create the local game session themselves pass none, and update() then does nothing (its first line). |
| `src/library/modules/runtime/savedata/sources/global/savedatahandler.cpp:334` | deviation: no Steam, no save data system |
| `src/library/modules/runtime/savedata/sources/win32/savedataprovider_win32_native.hpp:5` | SaveDataProviderWin32Steam stores in Steam's remote storage. OpenRFG uses it with --no-steam, --no-steam-cloud-saves or --save-dir (application.cpp deviation 4), so that such a run never touches the user's Steam Cloud saves. A write goes to a temporary file first and replaces the old file only ... |
| `src/library/modules/runtime/savedata/sources/win32/savedatasystem_win32.cpp:7` | Deviation (application.cpp deviation 4): rfg.exe always runs with Steam and the Steam provider and exits on any other type. OpenRFG also creates the local SaveDataSystemType_Win32Native, SaveDataProviderWin32Native (new code), and uses it unless ... |
| `src/library/modules/runtime/savedata/sources/win32/savedatasystem_win32.cpp:77` | 0x00CCEE00 (:50). Only the Steam system exists on Win32 in rfg.exe; another type exits. Deviation: the native system exists too. |
| `src/library/modules/runtime/savedata/sources/win32/steam/savedataprovider_win32_steam.cpp:102` | Deviation: retail signals the helper event before it requests the quit. The event is auto-reset, so a helper that wakes between the two (e.g. preempting this thread on the same core) consumes the signal, still sees no quit request, sleeps again, and the ... |

### keen/systemservices

| Location | Comment |
|---|---|
| `src/library/modules/runtime/systemservices/sources/global/systemservices.cpp:8` | Deviation (OpenRFG with --no-steam): rfg.exe always has system services (Steam's). Without Steam OpenRFG creates none (gameframework.cpp); the functions the game calls answer for the null system at their first line, as the stand-ins did. |
| `src/library/modules/runtime/systemservices/sources/global/systemservices.cpp:111` | deviation: no Steam |
| `src/library/modules/runtime/systemservices/sources/global/systemservices.cpp:141` | deviation: no Steam |
| `src/library/modules/runtime/systemservices/sources/global/systemservices.cpp:215` | deviation: no Steam |
| `src/library/modules/runtime/systemservices/sources/global/systemservices.cpp:306` | deviation: no Steam |
| `src/library/modules/runtime/systemservices/sources/global/systemservices.cpp:317` | deviation: no Steam |
| `src/library/modules/runtime/systemservices/sources/global/systemservices.cpp:348` | deviation: no Steam |
| `src/library/modules/runtime/systemservices/sources/steam/systemservices_steam.cpp:7` | Deviation (OpenRFG with --no-steam): see systemservices.cpp; the functions the game calls answer for the null system at their first line. |
| `src/library/modules/runtime/systemservices/sources/steam/systemservices_steam.cpp:150` | deviation: no Steam |
| `src/library/modules/runtime/systemservices/sources/steam/systemservices_steam.cpp:184` | deviation: no Steam |
| `src/library/modules/runtime/systemservices/sources/steam/systemservices_steam.cpp:333` | deviation: no Steam |
| `src/library/modules/runtime/systemservices/sources/steam/systemservices_steam.cpp:347` | deviation: no Steam |
| `src/library/modules/runtime/systemservices/sources/steam/systemservices_steam.cpp:357` | deviation: no Steam |

### keen/useraccount

| Location | Comment |
|---|---|
| `src/library/modules/runtime/useraccount/sources/steam/useraccountsystem_steam.cpp:7` | Deviation (stub_impl.md S2, OpenRFG with --no-steam): rfg.exe exits when Steam does not start, so it never calls |
| `src/library/modules/runtime/useraccount/sources/steam/useraccountsystem_steam.cpp:8` | these without a system. OpenRFG runs without Steam with --no-steam (application.cpp deviation 1); the queries that take the (then null) system answer "no account" at their first line, as the stand-ins did. |
| `src/library/modules/runtime/useraccount/sources/steam/useraccountsystem_steam.cpp:93` | deviation: no Steam |
| `src/library/modules/runtime/useraccount/sources/steam/useraccountsystem_steam.cpp:103` | deviation: no Steam |
| `src/library/modules/runtime/useraccount/sources/steam/useraccountsystem_steam.cpp:121` | deviation: no Steam |
| `src/library/modules/runtime/useraccount/sources/steam/useraccountsystem_steam.cpp:147` | deviation: no Steam |
| `src/library/modules/runtime/useraccount/sources/steam/useraccountsystem_steam.cpp:168` | deviation: no Steam |

### render_lib

| Location | Comment |
|---|---|
| `src/volition/render_lib/code/rl_mesh_data.cpp:133` | OpenRFG deviation: the message box is replaced by a log line. WCHAR wide_message[512]; keen::convertUTF8ToUTF16((uint16*)wide_message, sizeof(wide_message), message); MessageBoxW(nullptr, wide_message, L"Corrupted Data", 0x12010); |
| `src/volition/render_lib/code/rl_primitive_renderer.cpp:722` | OpenRFG deviation: the message box is replaced by a log line. MessageBoxA(nullptr, "Failed to initialize renderer.", "Error", 0); |
| `src/volition/render_lib/code/rl_render_lib.cpp:660` | OpenRFG deviation: the message box is replaced by a log line. MessageBoxA(nullptr, "Failed to initialize renderer.", "Error", 0); |
| `src/volition/render_lib/code/rl_render_lib.cpp:933` | Deviation (bug_blood_decals.md, rl_shader_fixup.h): the decal vertex shaders that use the UNORM vertex normal without decoding it, and the sky dome shader with the apex NaN, are created from a patched copy of the blob. |
| `src/volition/render_lib/code/rl_renderer_core.cpp:98` | OpenRFG deviation: the message box is replaced by a log line. MessageBoxA(nullptr, "Failed to initialize renderer.", "Error", 0); |
| `src/volition/render_lib/code/rl_renderer_core.cpp:119` | OpenRFG deviation: the message box is replaced by a log line. MessageBoxA(nullptr, "Failed to initialize renderer.", "Error", 0); |
| `src/volition/render_lib/code/rl_shader_fixup.h:15` | Deviation: the sky dome vertex shader rfg-skybox_s picks the hemisphere color row from the height above the dome centre, y = clamp(pos.y - Skybox_position.y, 0, 10) (the dome mesh rfg_skybox is a radius 10 hemisphere, its apex vertex at y = 10), as asin(y * ... |
| `src/volition/render_lib/code/rl_shader_library.cpp:323` | OpenRFG deviation: the message box is replaced by a log line. MessageBoxA(nullptr, "Failed to initialize shaders.", "Error", 0); |
| `src/volition/render_lib/code/rl_skin_instance.cpp:337` | OpenRFG deviation: the message box is replaced by a log line. MessageBoxA(nullptr, "Failed to initialize renderer.", "Error", 0); |

### rfg

| Location | Comment |
|---|---|
| `src/volition/rfg/code/rfg.cpp:1791` | OpenRFG deviation: the message box is replaced by a log line. MessageBoxA(nullptr, "Application may not be launched remotely.", "Error!", 0); |

### rfg/camera

| Location | Comment |
|---|---|
| `src/volition/rfg/code/camera/camera_cutscene.cpp:69` | Deviation: a stack buffer where rfg.exe prints to its thread-local one. |

### rfg/cutscene

| Location | Comment |
|---|---|
| `src/volition/rfg/code/cutscene/cutscene_options.cpp:56` | Deviation: a stack buffer where rfg.exe prints to its thread-local one. |
| `src/volition/rfg/code/cutscene/cutscene_options.cpp:277` | Deviation: a stack buffer where rfg.exe prints to its thread-local one. |

### rfg/effects

| Location | Comment |
|---|---|
| `src/volition/rfg/code/effects/effects.cpp:9` | Deviation: Effects_preload_srid and the singleplayer preload marks are file statics in rfg.exe; they have external linkage here because both parts use them. |

### rfg/gameplay

| Location | Comment |
|---|---|
| `src/volition/rfg/code/gameplay/gameflow/gfm_orders_manager.cpp:361` | Deviation: the headless programs can run without a player, and get_orders_location reads it; rfg.exe always has one. |
| `src/volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator.cpp:100` | Deviation: external (declared in the internal header) for gfm_orders_manager.cpp, which rfg.exe's unity build lets read it. |
| `src/volition/rfg/code/gameplay/activities/activity_courier_chase.cpp:558` | Deviation: rfg.exe inlines these and reads through null for a handle beyond the pool. |
| `src/volition/rfg/code/gameplay/activities/activity_courier_chase.cpp:735` | Deviation: rfg.exe inlines these and reads through null for a handle beyond the pool. |
| `src/volition/rfg/code/gameplay/activities/activity_mission_vrail.cpp:272` | Deviation: rfg.exe reads the handle through null when the spawn fails. |

### rfg/gamewide

| Location | Comment |
|---|---|
| `src/volition/rfg/code/gamewide/game_save_generic.cpp:50` | Deviation: twice rfg.exe's 257 entries. qsort_imported_save_data's slot pointer bug can walk up to about twice the row count, past the array and over the globals after it in rfg.exe; the spare entries only keep that inside this array. |
| `src/volition/rfg/code/gamewide/asset_assembler.cpp:138` | OpenRFG deviation: bounded (rfg.exe uses vsprintf). |
| `src/volition/rfg/code/gamewide/game_handshake.cpp:118` | Deviation: the headless programs have no render camera (camera_get_main_render_camera returns null without the renderer); there is then no deferred camera and handshake_deferred_render draws nothing. rfg.exe always has one. |
| `src/volition/rfg/code/gamewide/game_save_importer.cpp:7` | Deviation: OpenRFG with --no-steam has no Steam user, so getSaveGameImportDirectory answers an empty path (rfg.exe always has one, SteamUser would be null here) and the lookups find nothing. |
| `src/volition/rfg/code/gamewide/game_save_importer.cpp:24` | application.cpp, deviation 1 |
| `src/volition/rfg/code/gamewide/game_save_importer.cpp:168` | Deviation: no Steam user with --no-steam. |

### rfg/geom

| Location | Comment |
|---|---|
| `src/volition/rfg/code/geom/decal_info.cpp:3` | A split TU (deviation, VFX.md FX7, as effects.cpp in FX3): rfg compiles the table part (decal_info_system_init and the name lookups); rfg_effects compiles it again with OPENRFG_EFFECTS for the rest (the decal materials found per container, the renderer ... |

### rfg/hud

| Location | Comment |
|---|---|
| `src/volition/rfg/code/hud/hud_ui_weapon_cabinet.cpp:131` | Deviation: rfg.exe leaves the empty list's unlocked flag uninitialized. |
| `src/volition/rfg/code/hud/hud_ui_weapon_cabinet.cpp:491` | Deviation: for an empty list rfg.exe tests a stale stack slot here; the list is never empty on this screen, as hud_ui_weapon_cabinet_enter leaves at once. |

### rfg/interface

| Location | Comment |
|---|---|
| `src/volition/rfg/code/interface/main_menu.cpp:888` | Deviation (application.cpp deviation 1): online needs Steam, which --no-steam turns off. |
| `src/volition/rfg/code/interface/main_menu.h:66` | Test seams (UI.md 2.7 items 2 and 3, deviation 4; off by default, main_menu.cpp): the local player is caught at once (--notitle, implied by --host/--join); a function run at the end of each main menu frame once the menu exists. |
| `src/volition/rfg/code/interface/ui_memory_manager.cpp:361` | Deviation: rfg.exe calls _aligned_malloc directly, with alignment 0 from every caller (fog of war's materials, and always for the empty GPU block). Its MSVCR100 accepts that, as the caller uses the block unchecked; the UCRT raises an invalid parameter and ... |
| `src/volition/rfg/code/interface/ui_startup_video.cpp:188` | Deviation (UI.md deviation 4): OpenRFG's command line skips the table, as an empty list would (the pool stays locked for ui_startup_video_cleanup). |
| `src/volition/rfg/code/interface/ui_startup_video.h:38` | Test seam (UI.md deviation 4; off by default): OpenRFG's --notitle, --host and --join play no start-up video. |

### rfg/level

| Location | Comment |
|---|---|
| `src/volition/rfg/code/level/world.cpp:373` | Deviation: no class for the subtype means an empty list (see the file comment). |

### rfg/multi

| Location | Comment |
|---|---|
| `src/volition/rfg/code/multi/multi_core/global/multi_core_session_base.cpp:1213` | Port deviation (see multi_core_session_base.h), off by default. |
| `src/volition/rfg/code/multi/multi_core/include/multi_core_session_base.h:430` | Port deviation (multiplayer.md MP4; no rfg.exe counterpart): rfg.exe cannot start a system link match. The LAN lobby is created without filters (multi_session::platform_session_create), the game start sets the match state filter ... |
| `src/volition/rfg/code/multi/multi_game/multi_game_settings.cpp:1458` | Port deviation: rfg.exe leaves the mode options of a mode without any (team deathmatch) as stack garbage. |

### rfg/objects

| Location | Comment |
|---|---|
| `src/volition/rfg/code/objects/humans/human.h:768` | Deviation (not in rfg.exe): the regeneration delay a multiplayer client starts when the server's state lowers a player's hit points (human_damage applies no damage on a client, so rfg.exe never starts it there). |

### vlib

| Location | Comment |
|---|---|
| `src/volition/vlib/code/os/vconsole.cpp:11` | Deviation: codes past the 256 keys count as released here (rfg.exe reads unrelated memory there). - Entering a command compares it with Command_history[Command_history_last] before checking that the history has |
| `src/volition/vlib/code/os/vconsole.cpp:13` | an entry. Deviation: an empty history counts as a different command here (rfg.exe reads the memory before it). - The "!prefix" history recall and the history search compare case-insensitively up to the shorter length. |
