// Original: game/sources/application.cpp
//
// keen::rfg::Application, the game's keen GameApplication (see application.hpp for the threads and the boot steps).
//
// Build: the file has two complementary halves, so that the programs without the original entry keep only the input
// event writer:
// - Without OPENRFG_APPLICATION_MAIN (the rfg library, player_minimal.md P6): Application::processInputEvent, the writer
//   of vlib's input double buffer (Cached_input_states[Next_cached_input_state], input_internal.h). keen calls it on
//   the main thread for each input event; input_poll takes the buffer on the sim thread. It reads nothing of the
//   Application object, so a driver without the keen framework calls it on an Application-sized buffer (non-virtually)
//   with the events it makes up.
// - With OPENRFG_APPLICATION_MAIN (the rfg_app library, orig_main.md OM3, only OpenRFG links it): the rest, from
//   getGameApplication and fillGameframeworkParameters to the boot steps of render, the simulation thread and the
//   lockstep functions of the simulation thread's frame (start/end_middle_of_frame, lock/unlock_render).
//
// processInputEvent's pad branches (final_push.md step 6) ask keen's input system for the controller info, which the
// headless programs do not have; none of them produces pad events.
//
// Deviations (orig_main.md section 3), each commented at its line:
// - 1: useSteam is g_openrfgUseSteam, true as in rfg.exe; OpenRFG's --no-steam clears it and there is no Steam user
//   account or system services system.
// - 2: -gamebuild DIR mounts DIR as /gamebuild instead of <exe dir>/data.
// - 3: OpenRFG's --window-pos X Y replaces the window's fixed position.
// - 4: the save data is local (SaveDataProviderWin32Native) with --no-steam, --no-steam-cloud-saves or --save-dir; rfg.exe always uses
//   Steam's remote storage.
// The NVIDIA Highlights and Discord calls go to the shipped GfeSDK.dll and discord-rpc.dll (stub_impl.md S2).

#include "game/sources/application.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/vlib/code/input/input_internal.h"
#include "volition/vlib/code/os/os.h"

#if defined(OPENRFG_APPLICATION_MAIN)
#include "game/build/win32/steam/master/achievements/rfgachievements_adf.hpp"
#include "game/build/win32/steam/master/stats/rfgstats_stdf.hpp"
#include "kaiko/gamelink/gamelink.hpp"
#include "kaiko/gamesessioninteractions/gamesessioninteractions.hpp"
#include "keen/base/filepath.hpp"
#include "keen/base/filesystem.hpp"
#include "keen/base/nativefile.hpp"
#include "keen/base/profilingtimer.hpp"
#include "keen/base/string.hpp"
#include "keen/base/stringencoding.hpp"
#include "keen/base/trace.hpp"
#include "keen/gameframework/gameframework.hpp"
#include "keen/graphics/graphicssystem.hpp"
#include "keen/input/inputsystem.hpp"
#include "keen/savedata/savedatasystem.hpp"
#include "keen/savedata/win32/savedatasystem_win32.hpp"
#include "keen/useraccount/useraccountsystem.hpp"
#include "library/modules/thirdparty/discord-rpc/include/discord_rpc.h"
#include "unit4/gamesession/remotegamesession.hpp"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/hkbasesystem.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_primitive_renderer.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_options.h"
#include "volition/rfg/code/gamewide/game_save.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/havok/havok_bp.h"
#include "volition/rfg/code/havok/havok_memory.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/option_menus/ingame_options.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/nvidia_highlights.h"
#include "volition/rfg/code/rfg.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/cfile/keen_cfile.h"
#include "volition/vlib/code/cfile/packfile.h"
#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/memory/page_mempool.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/scheduler.h"
#include "volition/vlib/code/os/timer.h"

#include <string.h>
#include <windows.h>
#endif

#include <stdlib.h>

#if !defined(OPENRFG_APPLICATION_MAIN)
#include "keen/input/inputsystem.hpp"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/interface/ui_startup_video.h"

static bool g_ignoreFocusChange; // 0x01A37840

// size 0x2: a keen button index of a pad type -> vlib button (buttons_down index)
struct RfgButtonIdMapping
{
    unsigned char keenId; // 0x00
    unsigned char rfgId; // 0x01
};

// size 0x3: a keen axis index of a pad type -> vlib stick and its axis (0: x, 1: y)
struct RfgAxisIdMapping
{
    unsigned char keenId; // 0x00
    unsigned char rfgStickId; // 0x01
    unsigned char rfgAxisId; // 0x02
};

// size 0x2: a keen axis (the triggers) -> an analogue vlib button
struct RfgAxisToButtonMapping
{
    unsigned char keenId; // 0x00
    unsigned char rfgId; // 0x01
};

// 0x012B76E0 (.rdata): keen mouse button id -> vlib mouse button (0xFF: not used)
static const unsigned char s_rfgMouseIdMap[9] = {0, 1, 2, 0xFF, 0xFF, 0xFF, 0xFF, 3, 4};
// The pad tables (.rdata, rfg.exe's data). The Xbox 360, Xbox One and DualShock 4 tables are identical.
static const RfgButtonIdMapping s_rfgButtonMapping_xbox[14] = {{0, 10}, {1, 8}, {2, 9}, {3, 11}, {4, 0}, {5, 1}, {6, 3}, {7, 2}, {8, 4}, {10, 5}, {12, 14}, {13, 15}, {14, 12}, {15, 13}}; // 0x012B76EC
static const RfgAxisIdMapping s_rfgAxisMapping_xbox[4] = {{0, 0, 0}, {1, 0, 1}, {2, 1, 0}, {3, 1, 1}}; // 0x012B7708
static const RfgAxisToButtonMapping s_rfgAxisToButtonMapping_xbox[2] = {{4, 6}, {5, 7}}; // 0x012B7598
static const RfgButtonIdMapping s_rfgButtonMapping_xbone[14] = {{0, 10}, {1, 8}, {2, 9}, {3, 11}, {4, 0}, {5, 1}, {6, 3}, {7, 2}, {8, 4}, {10, 5}, {12, 14}, {13, 15}, {14, 12}, {15, 13}}; // 0x012B7714
static const RfgAxisIdMapping s_rfgAxisMapping_xbone[4] = {{0, 0, 0}, {1, 0, 1}, {2, 1, 0}, {3, 1, 1}}; // 0x012B7730
static const RfgAxisToButtonMapping s_rfgAxisToButtonMapping_xbone[2] = {{4, 6}, {5, 7}}; // 0x012B759C
static const RfgButtonIdMapping s_rfgButtonMapping_ps4[14] = {{0, 10}, {1, 8}, {2, 9}, {3, 11}, {4, 0}, {5, 1}, {6, 3}, {7, 2}, {8, 4}, {10, 5}, {12, 14}, {13, 15}, {14, 12}, {15, 13}}; // 0x012B773C
static const RfgAxisIdMapping s_rfgAxisMapping_ps4[4] = {{0, 0, 0}, {1, 0, 1}, {2, 1, 0}, {3, 1, 1}}; // 0x012B7758
static const RfgAxisToButtonMapping s_rfgAxisToButtonMapping_ps4[2] = {{4, 6}, {5, 7}}; // 0x012B7764
static const RfgButtonIdMapping s_rfgButtonMapping_switch[18] = {{0, 10}, {1, 8}, {2, 9}, {3, 11}, {4, 1}, {5, 0}, {6, 3}, {7, 2}, {8, 4}, {9, 6}, {10, 4}, {11, 5}, {12, 7}, {13, 5}, {14, 14}, {15, 15}, {16, 12}, {17, 13}}; // 0x012B7768
static const RfgAxisIdMapping s_rfgAxisMapping_switch[4] = {{0, 0, 0}, {1, 0, 1}, {2, 1, 0}, {3, 1, 1}}; // 0x012B778C
// Magic numbers: 22 and 23, the Steam controller's trigger axes, onto vlib's analogue buttons 103 and 104.
static const RfgAxisToButtonMapping s_rfgAxisToButtonMapping_steam[2] = {{22, 103}, {23, 104}}; // 0x012B7798

// The tail every pad write shares in rfg.exe: the pad is connected, with its keen controller and type.
static void rfg_note_controller(cached_controller_state& controller, const keen::InputEvent& inputEvent)
{
    controller.connected = true;
    controller.keen_controller_index = inputEvent.controllerId;
    keen::ControllerInfo info;
    keen::input::getControllerInfo(&info, inputEvent);
    controller.controller_type = info.type;
}

namespace keen
{
    namespace rfg
    {
        // 0x00458CF0 (:1467)
        // Only local player 0 (localPlayerId 0) feeds vlib's pad. A pad no player holds (0xFF) only feeds the start-up
        // videos' "press any button" and the debug free camera's axes.
        bool Application::processInputEvent(const InputEvent& inputEvent)
        {
            if (inputEvent.type == InputEventType_FocusLost)
            {
                if (!g_ignoreFocusChange)
                {
                    audiolib_mute_sound(true);
                    Os_app_active = false;
                }
            }
            else if (inputEvent.type == InputEventType_FocusRestored && !g_ignoreFocusChange)
            {
                audiolib_mute_sound(false);
                Os_app_active = true;
            }
            cached_input_state& state = Cached_input_states[Next_cached_input_state];
            switch (inputEvent.type)
            {
            case InputEventType_RawButtonDown:
            case InputEventType_RawButtonUp:
            {
                const float value = inputEvent.type == InputEventType_RawButtonDown ? 1.0f : 0.0f;
                if (inputEvent.controllerClass == ControllerClass_Keyboard)
                {
                    // Original: no range check on the key code (keen's key ids are below 256).
                    state.keyboard.keys[inputEvent.data.button.buttonIndex] = value != 0.0f;
                }
                else if (inputEvent.controllerClass == ControllerClass_Mouse)
                {
                    const uint32 id = inputEvent.data.button.buttonIndex;
                    if (id < 9 && s_rfgMouseIdMap[id] != 0xFF)
                    {
                        state.mouse.buttons[s_rfgMouseIdMap[id]] = value != 0.0f;
                    }
                }
                else if (inputEvent.controllerClass == ControllerClass_Controller)
                {
                    const uint32 buttonIndex = inputEvent.data.button.buttonIndex;
                    if (inputEvent.localPlayerId == 0xFF)
                    {
                        // A pad no player holds: a press skips the start-up videos.
                        if (value > 0.0f && inputEvent.controllerClass == ControllerClass_Controller)
                        {
                            ControllerInfo info;
                            input::getControllerInfo(&info, inputEvent);
                            if (info.type == ControllerType_Steam)
                            {
                                // Magic numbers: 0x47, 0x49 and 0x4A, Steam controller buttons (inputsystem.hpp's
                                // SteamControllerActionOrigin is not what the index holds; not decoded).
                                if (buttonIndex == 0x49 || buttonIndex == 0x4A || buttonIndex == 0x47)
                                {
                                    Pressing_any_button_for_startup_videos = true;
                                }
                            }
                            else
                            {
                                const StandardButtonId standard = inputEvent.data.button.standardButton;
                                if (standard == StandardButtonId_Confirm || standard == StandardButtonId_Exit || standard == StandardButtonId_StartPause)
                                {
                                    Pressing_any_button_for_startup_videos = true;
                                }
                                else
                                {
                                    input::getControllerInfo(&info, inputEvent);
                                    if (info.type == ControllerType_Unkown)
                                    {
                                        Pressing_any_button_for_startup_videos = true;
                                    }
                                }
                            }
                        }
                    }
                    else if (inputEvent.localPlayerId < 1)
                    {
                        cached_controller_state& controller = state.controllers[inputEvent.localPlayerId];
                        ControllerInfo info;
                        input::getControllerInfo(&info, inputEvent);
                        const RfgButtonIdMapping* mapping = nullptr;
                        uint32 count = 0;
                        switch (info.type)
                        {
                        case ControllerType_DualShock4:
                            mapping = s_rfgButtonMapping_ps4;
                            count = 14;
                            break;
                        case ControllerType_Xbox360:
                            mapping = s_rfgButtonMapping_xbox;
                            count = 14;
                            break;
                        case ControllerType_XBone:
                            mapping = s_rfgButtonMapping_xbone;
                            count = 14;
                            break;
                        case ControllerType_JoyConPair:
                        case ControllerType_SwitchProController:
                            mapping = s_rfgButtonMapping_switch;
                            count = 18;
                            break;
                        default:
                            // Other pads (the Steam controller among them): keen's index is vlib's button.
                            // Magic number: 0x80, the size of buttons_down.
                            if (buttonIndex < 0x80)
                            {
                                controller.buttons_down[buttonIndex] = value;
                                rfg_note_controller(controller, inputEvent);
                            }
                            break;
                        }
                        for (uint32 i = 0; i < count; i++)
                        {
                            if (mapping[i].keenId == buttonIndex && mapping[i].rfgId < 0x80)
                            {
                                controller.buttons_down[mapping[i].rfgId] = value;
                                rfg_note_controller(controller, inputEvent);
                                break;
                            }
                        }
                    }
                }
                break;
            }
            case InputEventType_RawAxis:
            {
                const uint32 axisIndex = inputEvent.data.axis.axisIndex;
                const float axisValue = inputEvent.data.axis.value;
                if (inputEvent.localPlayerId == 0xFF)
                {
                    // Magic number: 4, the debug free camera's axes (game_render.cpp).
                    if (inputEvent.controllerClass == ControllerClass_Controller && axisIndex < 4)
                    {
                        DebugAxisInput[axisIndex] = axisValue;
                    }
                    break;
                }
                if (inputEvent.localPlayerId >= 1)
                {
                    break;
                }
                cached_controller_state& controller = state.controllers[inputEvent.localPlayerId];
                ControllerInfo info;
                input::getControllerInfo(&info, inputEvent);
                const RfgAxisIdMapping* axisMapping = nullptr;
                const RfgAxisToButtonMapping* buttonMapping = nullptr;
                uint32 buttonCount = 0;
                switch (info.type)
                {
                case ControllerType_DualShock4:
                    axisMapping = s_rfgAxisMapping_ps4;
                    buttonMapping = s_rfgAxisToButtonMapping_ps4;
                    buttonCount = 2;
                    break;
                case ControllerType_Xbox360:
                    axisMapping = s_rfgAxisMapping_xbox;
                    buttonMapping = s_rfgAxisToButtonMapping_xbox;
                    buttonCount = 2;
                    break;
                case ControllerType_XBone:
                    axisMapping = s_rfgAxisMapping_xbone;
                    buttonMapping = s_rfgAxisToButtonMapping_xbone;
                    buttonCount = 2;
                    break;
                case ControllerType_JoyConPair:
                case ControllerType_SwitchProController:
                    axisMapping = s_rfgAxisMapping_switch;
                    break;
                case ControllerType_Steam:
                    buttonMapping = s_rfgAxisToButtonMapping_steam;
                    buttonCount = 2;
                    break;
                default:
                    break;
                }
                if (axisMapping != nullptr)
                {
                    // Magic numbers: 4 table entries, 0x20 sticks, 2 axes a stick.
                    for (uint32 i = 0; i < 4; i++)
                    {
                        const RfgAxisIdMapping& entry = axisMapping[i];
                        if (entry.keenId == axisIndex && entry.rfgStickId < 0x20 && entry.rfgAxisId < 2)
                        {
                            if (entry.rfgAxisId == 0)
                            {
                                controller.stick_values[entry.rfgStickId].x = axisValue;
                            }
                            else
                            {
                                controller.stick_values[entry.rfgStickId].y = axisValue;
                            }
                            rfg_note_controller(controller, inputEvent);
                            break;
                        }
                    }
                }
                else
                {
                    // Other pads: keen's axes in pairs, x then y of stick axisIndex / 2.
                    const uint32 stick = axisIndex >> 1;
                    if (stick < 0x20)
                    {
                        if ((axisIndex & 1) == 0)
                        {
                            controller.stick_values[stick].x = axisValue;
                        }
                        else
                        {
                            controller.stick_values[stick].y = axisValue;
                        }
                        rfg_note_controller(controller, inputEvent);
                    }
                }
                // The triggers also feed analogue buttons, clamped to [0, 1] (in double, as rfg.exe; NaN gives 0).
                if (buttonMapping != nullptr)
                {
                    for (uint32 i = 0; i < buttonCount; i++)
                    {
                        if (buttonMapping[i].keenId == axisIndex && buttonMapping[i].rfgId < 0x80)
                        {
                            float clamped = axisValue;
                            if (!((float)((double)clamped - 0.0) >= 0.0f))
                            {
                                clamped = 0.0f;
                            }
                            if ((float)((double)clamped - 1.0) >= 0.0f)
                            {
                                clamped = 1.0f;
                            }
                            controller.buttons_down[buttonMapping[i].rfgId] = clamped;
                            rfg_note_controller(controller, inputEvent);
                            break;
                        }
                    }
                }
                break;
            }
            case InputEventType_GyroAngularVelocity:
                if (inputEvent.localPlayerId != 0xFF && inputEvent.localPlayerId < 1)
                {
                    cached_controller_state& controller = state.controllers[inputEvent.localPlayerId];
                    controller.gyro_values[0].value = inputEvent.data.gyro.angularVelocity.x;
                    controller.gyro_values[1].value = inputEvent.data.gyro.angularVelocity.y;
                    controller.gyro_values[2].value = inputEvent.data.gyro.angularVelocity.z;
                    rfg_note_controller(controller, inputEvent);
                }
                break;
            case InputEventType_GyroOrientation:
                if (inputEvent.localPlayerId != 0xFF && inputEvent.localPlayerId < 1)
                {
                    cached_controller_state& controller = state.controllers[inputEvent.localPlayerId];
                    controller.gyro_values[3].value = inputEvent.data.gyro.orientation.x;
                    controller.gyro_values[4].value = inputEvent.data.gyro.orientation.y;
                    controller.gyro_values[5].value = inputEvent.data.gyro.orientation.z;
                    controller.gyro_values[6].value = inputEvent.data.gyro.orientation.w;
                    rfg_note_controller(controller, inputEvent);
                }
                break;
            case InputEventType_MouseMove:
                state.mouse.pos_x = inputEvent.data.mouse.position.x;
                state.mouse.pos_y = inputEvent.data.mouse.position.y;
                break;
            case InputEventType_MouseRelativeMovement:
                // accumulated until input_poll takes the buffer
                state.mouse.relative_move_x = inputEvent.data.mouse.position.x + state.mouse.relative_move_x;
                state.mouse.relative_move_y = inputEvent.data.mouse.position.y + state.mouse.relative_move_y;
                break;
            case InputEventType_MouseWheel:
                state.mouse.relative_move_wheel = (float)inputEvent.data.wheel.wheelDelta + state.mouse.relative_move_wheel;
                break;
            default:
                break;
            }
            return true;
        }
    }
}
#endif // !OPENRFG_APPLICATION_MAIN

#if defined(OPENRFG_APPLICATION_MAIN)
// ---- globals of application.cpp
keen::GraphicsSystem* g_game_options_graphicsSystem; // 0x02505508, the graphics system the options menus switch modes on
// Deviation 1 (application.hpp): on as in rfg.exe, off when OpenRFG runs with --no-steam.
bool g_openrfgUseSteam = true;
bool g_openrfgSteamCloudSaves = true;
const char* g_openrfgSaveDirectory = nullptr;
// Deviation 3 (application.hpp): off unless OpenRFG runs with --window-pos.
bool g_openrfgWindowPosSet = false;
int g_openrfgWindowPosX = 0;
int g_openrfgWindowPosY = 0;

// 0x012B8168 (.rdata): the 89 actions of the Steam controller configuration (keen's InputSystemCreationParameters). The
// Steam controller is stubbed (orig_main.md 2.5), so only the count matters; the names are not recovered.
static const keen::SteamControllerAction actions[0x59] = {}; // 0x012B8168

// ---- inline constructors of the framework headers, emitted in this TU
namespace keen
{
    namespace SaveData
    {
        // 0x00458A30 (savedatasystem.hpp:129)
        SaveDataSystemCreationParameters::SaveDataSystemCreationParameters()
        {
            pSaveDataFormats = nullptr;
            saveDataSystemType = SaveDataSystemType_Invalid;
            suppressSystemDialogs = false;
            pGlobalSaveDataFormats = nullptr;
            globalsaveDataFormatCount = 0;
        }

        // 0x00458A50 (savedatasystem_win32.hpp:17)
        SaveDataSystemCreationProviderParametersWin32::SaveDataSystemCreationProviderParametersWin32()
        {
            pSteamworksSystem = nullptr;
            pApplicationName = nullptr;
            pSaveDirectory = nullptr;
        }
    }
}

namespace unit4
{
    // 0x00459730 (systemservices.hpp:79)
    SystemServicesParameters::SystemServicesParameters()
    {
        credentials = SystemServicesCredentials();
        achievementCount = 0;
        presenceStringCount = 0;
        rankingTableCount = 0;
        rankingCacheCount = 1;
        rankingResultSetSize = 0;
        rankingSendQueueSize = 3;
        platinumAchievementId = 0xFFFFFFFF;
        pSteamworksSystem = nullptr;
    }
}

// fillSavedataParameters (function-local statics in rfg.exe, file statics here): one save data format of one segment,
// whose size is the game save's size.
static uint32 segmentSizes[1]; // 0x01A378E0
static const keen::SaveData::SaveDataFormat formats[1] = {{1, 1, 1, segmentSizes}}; // 0x012B8100
static keen::SaveData::SaveDataSystemCreationProviderParametersWin32 win32Params; // 0x01A378E4
static keen::SaveData::SaveDataSystemCreationParameters saveDataSystemParameters; // 0x01A378EC
// fillOnlineSystemParameters (a function-local static in rfg.exe)
static unit4::SystemServicesParameters systemServiceParams; // 0x01A37918
static const char* s_presenceStrings[1] = {"Idle"}; // 0x014F56F0

namespace
{
    // Deviation 2 (orig_main.md section 3, off by default): -gamebuild DIR mounts DIR as /gamebuild instead of
    // <exe dir>/data, so the program runs outside the game folder (the tests' data directory).
    const char* find_gamebuild_override(const keen::GameApplicationParameters& parameters)
    {
        for (int i = 1; i + 1 < parameters.argc; i++)
        {
            if (strcmp(parameters.argv[i], "-gamebuild") == 0)
            {
                return parameters.argv[i + 1];
            }
        }
        return nullptr;
    }

    // Deviation 3: --window-pos gives physical pixels, but this process is not DPI aware (as rfg.exe), so Windows scales its
    // window coordinates by the monitor's DPI. The monitor under the point has one rectangle per awareness, which gives the
    // mapping (a monitor's scale and origin both differ). A DPI aware process gets the same rectangle twice, so no change.
    POINT window_pos_from_physical(int x, int y)
    {
        POINT point = {x, y};
        // Looked up by name: Windows 10 1703 and later; older systems keep the coordinates as given.
        typedef void*(WINAPI * set_thread_dpi_awareness_context)(void*);
        const set_thread_dpi_awareness_context set_context = (set_thread_dpi_awareness_context)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetThreadDpiAwarenessContext");
        if (set_context == nullptr)
        {
            return point;
        }
        // Magic number: -4, DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2.
        void* const previous = set_context((void*)-4);
        if (previous == nullptr)
        {
            return point;
        }
        MONITORINFO physical = {};
        physical.cbSize = sizeof(physical);
        const HMONITOR monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
        GetMonitorInfoW(monitor, &physical);
        set_context(previous);
        MONITORINFO scaled = {};
        scaled.cbSize = sizeof(scaled);
        GetMonitorInfoW(monitor, &scaled);
        const RECT& from = physical.rcMonitor;
        const RECT& to = scaled.rcMonitor;
        point.x = to.left + MulDiv(x - from.left, to.right - to.left, from.right - from.left);
        point.y = to.top + MulDiv(y - from.top, to.bottom - to.top, from.bottom - from.top);
        return point;
    }
}

namespace keen
{
    // 0x0045A850 (:1131)
    // The application singleton (s_appInstance, 0x01A37A58, a function-local static constructed on the first call).
    GameApplication& getGameApplication()
    {
        static rfg::Application s_appInstance; // 0x01A37A58
        return s_appInstance;
    }

    namespace rfg
    {
        Application* Application::m_staticApplicationPointer; // 0x01A37844

        // The lockstep functions reach the singleton through getGameApplication (its construction guard is inlined in
        // rfg.exe).
        static Application& get_application()
        {
            return static_cast<Application&>(getGameApplication());
        }

        // The render lock of the calling thread (a thread local in rfg.exe, offset 0x10 of the TLS block).
        static __declspec(thread) bool s_rendering_locked;

        // 0x00459C80 (:115)
        Application::Application()
        {
            m_rfgGameInitializationStep = 0;
            m_rfgGameInitializeThreadDone = false;
            m_userAccountOperationHandle = 1;
            m_gameSessionMsgboxHandle = -1;
            m_gameSessionMsgboxSelection = 0xFFFFFFFF;
            memset(&m_gameSessionLastInteraction, 0, sizeof(m_gameSessionLastInteraction));
            m_staticApplicationPointer = this;
        }

        // 0x004597A0 (:122)
        Application::~Application()
        {
        }

        // 0x00459D80 (:126)
        void Application::fillGameframeworkParameters(GameFrameworkParameters* pParameters, const GameApplicationParameters&)
        {
            pParameters->pProjectName = "Red Faction Guerrilla Re-Mars-tered";
            pParameters->fileSystemParameters.maxDeviceCount = 8;
            pParameters->fileSystemParameters.useReadThreads = true;
            pParameters->initializeSoundSystem = false;
            pParameters->maxTaskSystemWorkerThreads = 0;
            pParameters->inputSystemCreationParameters.enableGyroSupport = true;
            pParameters->pProjectGameBuildDirectory = "C:/unit4projects/rfg/root/gamebuild/win32";
            copyString(m_nativeGamebuildPath, sizeof(m_nativeGamebuildPath), "C:/unit4projects/rfg/root/gamebuild/win32");
            pParameters->inputSystemCreationParameters.pControllerActions = actions;
            // Magic number: 0x59, the Steam controller actions.
            pParameters->inputSystemCreationParameters.controllerActionCount = 0x59;
            pParameters->pRegionCode = "";
            // Deviation 1 (orig_main.md 2.5, stub_impl.md S2): rfg.exe sets useSteam = true, and GameFramework::initialize
            // exits when Steam does not start. Here it is the --no-steam seam (g_openrfgUseSteam); with it the original
            // Steam code runs without a Steam system (no user accounts, save data or system services; the commented
            // "deviation: no Steam" guards).
            pParameters->useSteam = g_openrfgUseSteam;
            // Magic number: 0xA3048, the game's Steam app id (667720).
            pParameters->steamAppId = 0xA3048;
            fillSavedataParameters(pParameters);
            fillOnlineSystemParameters(pParameters);
            // Magic numbers: a resizeable 1280x720 window at (60, 60), 960x540 when the desktop is smaller, its aspect
            // ratio between 1.25 and 2.333.
            pParameters->aspectRatioConstraintData.minAspectRatio = 1.25f;
            pParameters->resizeableWindow = true;
            pParameters->aspectRatioConstraintData.isConstrained = true;
            pParameters->aspectRatioConstraintData.maxAspectRatio = 2.3333333f;
            pParameters->screenWidth = 1280;
            pParameters->screenHeight = 720;
            pParameters->windowPosX = 60;
            pParameters->windowPosY = 60;
            // Deviation 3 (--window-pos): the framework's position is the client area's and AdjustWindowRect then grows the
            // frame around it, so the frame's size is added back: X Y is the frame's corner and (0, 0) keeps the caption on
            // screen. WS_OVERLAPPEDWINDOW is the framework's style for this resizeable window (initializeBase).
            if (g_openrfgWindowPosSet)
            {
                const POINT position = window_pos_from_physical(g_openrfgWindowPosX, g_openrfgWindowPosY);
                RECT frame = {};
                AdjustWindowRect(&frame, WS_OVERLAPPEDWINDOW, FALSE);
                pParameters->windowPosX = position.x - frame.left;
                pParameters->windowPosY = position.y - frame.top;
            }
            // Magic number: icon resource 101.
            pParameters->windowIcon = MAKEINTRESOURCEW(101);
            pParameters->isStandardIcon = false;
            RECT desktop;
            GetClientRect(GetDesktopWindow(), &desktop);
            if (desktop.right - desktop.left < (int)pParameters->screenWidth || desktop.bottom - desktop.top < (int)pParameters->screenHeight)
            {
                pParameters->screenWidth = 960;
                pParameters->screenHeight = 540;
            }
            pParameters->fileSystemParameters.maxDeviceCount = 8;
            // Magic number: 200 native streams.
            pParameters->fileSystemParameters.maxNativeStreamCount = 200;
            pParameters->fileSystemParameters.useReadThreads = false;
            game_options_load_ini_file_into_cache();
            rl_init_params init_params;
            bool vsync_enabled = false;
            int camera_fov = 0;
            if (game_options_load_display_options(init_params.m_screen_info, init_params.m_lod, vsync_enabled, camera_fov))
            {
                pParameters->screenWidth = init_params.m_screen_info.get_screen_width();
                pParameters->screenHeight = init_params.m_screen_info.get_screen_height();
            }
        }

        // 0x004594C0 (:1822)
        // The function-local statics of rfg.exe are file statics here (no guard is needed: they are constant after the
        // first call).
        void Application::fillSavedataParameters(GameFrameworkParameters* pParameters)
        {
            saveDataSystemParameters.suppressSystemDialogs = false;
            saveDataSystemParameters.pProviderParameters = (SaveData::SaveDataSystemCreationProviderParameters*)&win32Params;
            // Deviation 4: Steam's save data unless the command line turns it off. rfg.exe takes it whenever it is available, which is always.
            if (g_openrfgUseSteam && g_openrfgSteamCloudSaves && SaveData::isSaveDataSystemTypeAvailable(SaveData::SaveDataSystemType_Steam))
            {
                saveDataSystemParameters.saveDataSystemType = SaveData::SaveDataSystemType_Steam;
            }
            else
            {
                saveDataSystemParameters.saveDataSystemType = SaveData::SaveDataSystemType_Win32Native;
                win32Params.pApplicationName = "rfg";
                win32Params.pSaveDirectory = g_openrfgSaveDirectory;
            }
            static bool s_segmentSizeSet = false;
            if (!s_segmentSizeSet)
            {
                s_segmentSizeSet = true;
                segmentSizes[0] = game_save_get_save_interface_for_game_session()->getSaveDataSize();
            }
            saveDataSystemParameters.pSaveDataFormats = formats;
            saveDataSystemParameters.saveDataFormatCount = 1;
            saveDataSystemParameters.maxProfileCount = 1;
            pParameters->pSaveDataSystemParameters = &saveDataSystemParameters;
        }

        // 0x00459B90 (:1876)
        void Application::fillOnlineSystemParameters(GameFrameworkParameters* pParameters)
        {
            systemServiceParams.pAchievementIdMap = getAchievementIdMap();
            systemServiceParams.pAchievementUnlockCountMap = getAchievementUnlockValueMap();
            // Magic number: 61 achievements.
            systemServiceParams.achievementCount = 0x3D;
            systemServiceParams.platinumAchievementId = getPlatinumAchievementId();
            systemServiceParams.pStatUpdateMap = kaiko::getStatUpdateDataMap();
            systemServiceParams.statUpdateCount = kaiko::getStatCount();
            systemServiceParams.pTrackedChunksMap = rfg_get_tracked_chunk_ids();
            systemServiceParams.trackedChunksCount = rfg_get_num_tracked_chunk_ids();
            // Magic numbers: one presence string, 23 ranking tables, 2 caches of 101 results, a send queue of 8.
            systemServiceParams.presenceStringCount = 1;
            systemServiceParams.rankingTableCount = 0x17;
            systemServiceParams.rankingCacheCount = 2;
            systemServiceParams.rankingResultSetSize = 0x65;
            systemServiceParams.rankingSendQueueSize = 8;
            systemServiceParams.pPresenceStrings[0] = s_presenceStrings[0];
            pParameters->pSystemServicesParameters = &systemServiceParams;
        }

        // 0x00459F30 (:412)
        void Application::initialize(GameFrameworkSystem* pGameFramework, const GameApplicationParameters& parameters)
        {
            debug::nativePrintArgs("[WIN CONFIG] Config: %s Network: %s \n", "STEAM", "EMGIEC");
            m_lastControllerClass = ControllerClass_None;
            g_gameLinkMutex.create("GameLinkMutexRFG");
            vlib_timer_init();
            profiler::initializeTimer();
            profiler::updateTimer();
            g_game_options_graphicsSystem = GameFramework::getGraphicsSystem(pGameFramework);
            rfg_init_keen_memory_allocators(pGameFramework);
            FileSystem* pFileSystem = GameFramework::getFileSystem(pGameFramework);
            m_nativeGamebuildPath[0] = '\0';
            // Deviation 4 (orig_main.md section 3): the backtrace upload never sends anything (backtraceuploader.cpp).
            GameFramework::setBacktraceURL("http://rfg.sp.backtrace.io:6097/post?format=minidump&token=ff5af6071dc8f05b34ab7a8a616f2d6fcd953824996d78acfada638fa4721ac9");
            FilePath gamebuildPath;
            getCurrentExePath(&gamebuildPath);
            gamebuildPath.setFileNameWithExtension("");
            gamebuildPath.pushDirectory("data");
            const char* pGamebuild = gamebuildPath.getCompletePath();
            // Deviation 2 (orig_main.md 2.6): -gamebuild DIR.
            if (const char* pOverride = find_gamebuild_override(parameters))
            {
                pGamebuild = pOverride;
            }
            file::mountNativePath(pFileSystem, "/gamebuild", pGamebuild, false);
            copyString(m_nativeGamebuildPath, sizeof(m_nativeGamebuildPath), pGamebuild);
            keen_cf_set_filesystem(GameFramework::getFileSystem(pGameFramework));
            GraphicsSystem* pGraphicsSystem = GameFramework::getGraphicsSystem(pGameFramework);
            rl_init_params init_params;
            bool vsync_enabled = false;
            int camera_fov = 0;
            if (game_options_load_display_options(init_params.m_screen_info, init_params.m_lod, vsync_enabled, camera_fov))
            {
                const rl_screen_info& screen = init_params.m_screen_info;
                if (screen.get_window_mode() == graphics::WindowMode_Windowed)
                {
                    graphics::switchToWindowedMode(pGraphicsSystem, nullptr);
                }
                else
                {
                    int rr_num;
                    int rr_den;
                    screen.get_refresh_rate(rr_num, rr_den);
                    graphics::switchToFullScreenMode(pGraphicsSystem, nullptr, screen.get_screen_width(), screen.get_screen_height(), rr_num, rr_den, screen.get_window_mode());
                }
                graphics::setSwapChainPresentationInterval(pGraphicsSystem, nullptr, screen.get_vsync_enabled() ? 1 : 0);
            }
            localization_set_keen_system_language(GameFramework::getSystemLanguage(pGameFramework));
            SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(0x7F00))); // IDC_ARROW
            m_localGameSession.create(GameFramework::getDefaultAllocator(pGameFramework), GameFramework::getSaveDataSystem(pGameFramework), GameFramework::getSystemServices(pGameFramework),
                                      GameFramework::getUserAccountSystem(pGameFramework), 1, game_save_get_save_interface_for_game_session(), GameFramework::getInputSystem(pGameFramework));
            rfg_set_local_game_session(&m_localGameSession);
            m_pUserAccountSystem = GameFramework::getUserAccountSystem(pGameFramework);
            m_pInputSystem = GameFramework::getInputSystem(pGameFramework);
            m_pSystemServices = GameFramework::getSystemServices(pGameFramework);
            Keen_input_system = m_pInputSystem;
            input::setControllerApiHidden(m_pInputSystem, WindowsControllerApi_XInput, game_options_get_last_options_from_save().new_options.hide_xinput != 0);
            input::setControllerApiHidden(m_pInputSystem, WindowsControllerApi_DInput, game_options_get_last_options_from_save().new_options.hide_dinput != 0);
            input::setControllerApiHidden(m_pInputSystem, WindowsControllerApi_SteamController, game_options_get_last_options_from_save().new_options.hide_steam_controller != 0);
            m_pGameframework = pGameFramework;
            m_rfgGameInitializationStep = 0;
            // The sound system: 64 voices and virtual sounds, 32 temporary sound definitions, two busses into the master
            // out (-1) with the other targets unused (-2).
            SoundSystem::BusDefinition busses[2];
            for (SoundSystem::BusDefinition& bus : busses)
            {
                bus.targetBusses[0] = -1;
                bus.targetBusses[1] = -2;
                bus.targetBusses[2] = -2;
                bus.targetBusses[3] = -2;
            }
            SoundSystem::InitializationParameters soundParameters;
            soundParameters.pBusses = busses;
            soundParameters.virtualSoundCount = 0x40;
            soundParameters.voiceCount = 0x40;
            soundParameters.temporarySoundDefinitionCount = 0x20;
            soundParameters.busCount = 2;
            m_pSoundSystem = SoundSystem::initialize(GameFramework::getDefaultAllocator(pGameFramework), soundParameters);
            rfg_set_SoundSystem(m_pSoundSystem);
            m_simulationSignal.create("simulationSignal", 0);
            m_renderSignal.create("renderSignal", 0);
            m_renderMutex.create("renderSignal"); // the mutex has the render signal's name in rfg.exe
            // Magic numbers: priority 200, a 4 MB stack.
            m_simulationThread.create("simulation", runSimulation, 200, 0x400000, 0);
            m_simulationThread.start(this);
            m_simulationDone = false;
            m_hasDoneSimulationFrame = false;
            nvidia_highlights_init();
            DiscordEventHandlers handlers;
            memset(&handlers, 0, sizeof(handlers));
            handlers.ready = multi_core_platform_discord_ready;
            handlers.disconnected = multi_core_platform_discord_disconnected;
            handlers.errored = multi_core_platform_discord_errored;
            handlers.joinGame = multi_core_platform_discord_join_game;
            handlers.spectateGame = multi_core_platform_discord_spectate_game;
            handlers.joinRequest = multi_core_platform_discord_join_request;
            char steamAppId[0x40];
            formatString(steamAppId, sizeof(steamAppId), "%u", 0xA3048);
            // Magic number: the game's Discord application id.
            Discord_Initialize("469429746007015426", &handlers, 1, steamAppId);
        }

        // 0x00459F20 (:372)
        int Application::runSimulation(const Thread& thread)
        {
            return ((Application*)thread.getArgument())->updateSimulation(thread);
        }

        // 0x004597F0 (:377)
        // The simulation thread: it waits for render step 4, takes the pools and its own Havok memory router, then runs
        // the game frames until the main thread asks it to quit.
        int Application::updateSimulation(const Thread& thread)
        {
            m_simulationSignal.sleepUntilSignaled();
            rl_primitive_renderer::get_instance()->initialize_for_thread();
            packfile_set_thread_ownership();
            stream2_set_thread_ownership();
            havok_bpo_set_thread_ownership();
            Permanent_mempool.set_thread_ownership();
            Common_mempool.set_thread_ownership();
            Interface_mempool.set_thread_ownership();
            Interface_perm_mempool.set_thread_ownership();
            Effect_preload_mempool.set_thread_ownership();
            Material_effects_mempool.set_thread_ownership();
            if (Table_preload_pool_ptr != nullptr)
            {
                Table_preload_pool_ptr->set_thread_ownership();
            }
            hkMemoryRouter* memoryRouter = havok_create_thread_memory_router();
            hkBaseSystem::initThread(memoryRouter);
            RFG_scratch_pool.set_thread_ownership();
            sim_scratch_system_init();
            scheduler_register_sim_thread();
            vint_set_thread_ownership();
            while (!thread.isQuitRequested())
            {
                do_game_main_loop_frame();
            }
            return 0;
        }

        // 0x0045A880 (:711)
        void Application::updateTime(float)
        {
            nvidia_highlights_poll();
            nvidia_highlights_ask_user_for_permission();
            // Until the simulation thread runs (render step 5), the main thread does its session work.
            if (m_rfgGameInitializationStep < 5)
            {
                updateFromSimulationThread();
            }
            mouse_generic_update_front_thread();
        }

        // 0x0045A370 (:753)
        // The session work of a frame (the simulation thread's start_middle_of_frame once the boot is done): the local
        // game session and its interactions as message boxes, the user account operation, a local player that left,
        // a pending invitation, the primary controller class and the Discord callbacks.
        void Application::updateFromSimulationThread()
        {
            const float timeStep = Frametime_real;
            vlib_timer_update(timeStep);
            m_localGameSession.update(timeStep);
            unit4::SessionInteractionData interaction;
            memset(&interaction, 0, sizeof(interaction));
            const bool required = m_localGameSession.isInteractionRequired(&interaction);
            if (interaction.id == (uint32)kaiko::GameSessionInteractionId_WaitingForProcess || !required)
            {
                if (m_gameSessionMsgboxHandle != -1)
                {
                    debug::nativePrintArgs("Interaction no longer required.\n");
                    ui_hide_msgbox(m_gameSessionMsgboxHandle, false);
                    m_gameSessionMsgboxHandle = -1;
                }
                m_gameSessionLastInteraction = interaction;
            }
            else
            {
                const uint32 lastId = m_gameSessionLastInteraction.id;
                // Magic number: -2, any message box.
                if ((interaction.id != lastId || (lastId == (uint32)kaiko::GameSessionInteractionId_None && !ui_is_msgbox_active(-2))) && rfg_get_error_viewer_response() == 0 &&
                    rfg_get_currently_handled_error_viewer_event_id() == 0)
                {
                    if (m_gameSessionMsgboxHandle != -1)
                    {
                        ui_hide_msgbox(m_gameSessionMsgboxHandle, false);
                        m_gameSessionMsgboxHandle = -1;
                        debug::nativePrintArgs("Interaction changed, hiding original interaction\n");
                    }
                    m_gameSessionMsgboxSelection = 0xFFFFFFFF;
                    for (uint32 i = 0; i < interaction.params.parameterCount; i++)
                    {
                        rfg_process_game_session_interaction_param(interaction.params.parameters[i]);
                    }
                    // Magic number: ids above 999999 are error viewer events.
                    const bool errorViewer = interaction.id > 999999;
                    if (errorViewer)
                    {
                        interaction.id = (uint32)kaiko::GameSessionInteractionId_ErrorViewerRequired;
                    }
                    kaiko::GameSessionInteractionLocaTag locaTag;
                    kaiko::generateGameSessionInteractionLocaTag(locaTag, (kaiko::GameSessionInteractionId)interaction.id);
                    const wchar_t* pText = localization_get_display_string_req(locaTag.name, nullptr);
                    if (pText != nullptr)
                    {
                        kaiko::GameSessionInteractionFormattedDescriptionText description;
                        convertUTF16ToUTF8(description.text, sizeof(description.text), (const uint16*)pText);
                        kaiko::fillVariableGameSessionInteractionParams(description, interaction.params);
                        debug::nativePrintArgs("Starting interaction %s\n", locaTag.name);
                        int handle;
                        if (interaction.responseOptionCount == 0 || errorViewer)
                        {
                            handle = ui_add_msgbox(MBT_CONFIG_CONTROL, "LOCAL_GAME_SESSION_ERROR_TITLE", description.text, handleGameSessionMsgboxResponse, false, false, nullptr, nullptr, false);
                        }
                        else
                        {
                            handle = ui_add_msgbox(MBT_CONFIG_CONTROL, "LOCAL_GAME_SESSION_ERROR_TITLE", description.text, handleGameSessionMsgboxResponse, false, true, "MENU_ACCEPT", nullptr, false);
                            for (uint32 i = 0; i < interaction.responseOptionCount; i++)
                            {
                                kaiko::GameSessionInteractionLocaTag responseTag;
                                kaiko::generateGameSessionResponseLocaTag(responseTag, (kaiko::GameSessionResponseId)interaction.responseOptions[i]);
                                const wchar_t* pResponse = localization_get_display_string_req(responseTag.name, nullptr);
                                if (pResponse == nullptr)
                                {
                                    pResponse = L" ";
                                }
                                kaiko::GameSessionInteractionFormattedDescriptionText caption;
                                convertUTF16ToUTF8(caption.text, sizeof(caption.text), (const uint16*)pResponse);
                                kaiko::fillVariableGameSessionInteractionParams(caption, interaction.params);
                                ui_msgbox_add_caption_item(handle, caption.text, false, false);
                            }
                        }
                        if (handle != -1)
                        {
                            m_gameSessionMsgboxHandle = handle;
                            m_gameSessionLastInteraction = interaction;
                        }
                    }
                }
                if (m_gameSessionMsgboxSelection != 0xFFFFFFFF)
                {
                    m_localGameSession.setInteractionResponse(m_gameSessionLastInteraction.responseOptions[m_gameSessionMsgboxSelection]);
                    if (m_gameSessionMsgboxHandle != -1)
                    {
                        ui_hide_msgbox(m_gameSessionMsgboxHandle, false);
                        m_gameSessionMsgboxHandle = -1;
                        debug::nativePrintArgs("Interaction ended with %d\n", m_gameSessionMsgboxSelection);
                    }
                    memset(&m_gameSessionLastInteraction, 0, sizeof(m_gameSessionLastInteraction));
                }
            }
            if (m_userAccountOperationHandle != 1 && useraccount::getUserAccountOperationState(m_pUserAccountSystem, m_userAccountOperationHandle) != UserAccountAsyncOperationState_Busy)
            {
                useraccount::getUserAccountOperationResult(m_pUserAccountSystem, m_userAccountOperationHandle);
                m_userAccountOperationHandle = 1;
            }
            // Local player 0 (the id is a null pointer) left: back to the main menu, outside a level load.
            if (m_localGameSession.didLocalPlayerExit(nullptr) && gameseq_get_state() != GS_LOAD)
            {
                m_localGameSession.removeLocalPlayer(nullptr);
                input::clearControllerPlayerId(m_pInputSystem, nullptr);
                input::setControllerAutoCatch(m_pInputSystem, nullptr, InputSystemControllerAutoCatchType_AnyButton);
                main_menu_clear_auto_catch_done();
                ingame_options_exit_to_main_menu();
                wrecking_crew_should_restart(false);
                get_remote_game_session()->clearRequiredConfirmationInteraction();
                main_menu_notify_profile_change();
                multi_core_platform_clear_pending_invite();
                return;
            }
            g_gameLinkMutex.lock(0);
            const kaiko::GameLinkInvitationData* pInvitation = kaiko::gamelink::getInvitationData();
            if (pInvitation != nullptr)
            {
                // Original: only the invitation data is set; the flags of the local stay as the stack had them.
                pending_invite_info invite;
                invite.invitation_data = *pInvitation;
                multi_core_platform_set_pending_invite(&invite);
            }
            const ControllerClass controllerClass = input::getPrimaryInputDeviceClass(Keen_input_system, nullptr);
            if (controllerClass != m_lastControllerClass)
            {
                controls_set_control_config();
                if (controllerClass == ControllerClass_Controller)
                {
                    multi_core_platform_set_active_controller(0);
                    const ControllerType controllerType = input::getPrimaryInputDeviceType(Keen_input_system, nullptr, ControllerClass_Controller);
                    Cached_input_states[Next_cached_input_state].controllers[0].controller_type = controllerType;
                    input_set_playing_with_controller(true, controllerType);
                }
                else
                {
                    Cached_input_states[Next_cached_input_state].controllers[0].controller_type = ControllerType_Keyboard;
                    input_set_playing_with_controller(false, ControllerType_Keyboard);
                }
                controls_set_custom_from_current();
                wrecking_crew_reset_controls();
                m_lastControllerClass = controllerClass;
            }
            Discord_RunCallbacks();
            g_gameLinkMutex.unlock();
        }

        // 0x00459970 (:1038)
        // The main thread's frame: the boot steps 0-4, then (step 5) the lockstep with the simulation thread around
        // do_game_render_frame. The quit check runs after every step.
        void Application::render(GraphicsCommandWriter* pGraphicsCommandWriter) const
        {
            // rfg.exe's render is const, but the boot steps change the application.
            Application* pThis = const_cast<Application*>(this);
            rfg_update_resource_sizes(pGraphicsCommandWriter);
            bool quit = false;
            // Magic number: 4, the last boot step.
            if (m_rfgGameInitializationStep > 4)
            {
                rfg_set_simulationThreadCommandWriter(pGraphicsCommandWriter);
                pThis->m_simulationSignal.signal();
                pThis->m_renderSignal.sleepUntilSignaled();
                pThis->m_renderMutex.lock(0);
                rfg_set_simulationThreadCommandWriter(nullptr);
                do_game_render_frame(pGraphicsCommandWriter);
                pThis->m_renderMutex.unlock();
            }
            else
            {
                switch (m_rfgGameInitializationStep)
                {
                case 0:
                {
                    const int result = rfg_pre_initialize_game(m_pGameframework, pGraphicsCommandWriter);
                    pThis->m_rfgGameInitializationStep++;
                    quit = result != 0;
                    break;
                }
                case 1:
                    if (rfg_init_stage_1_loop_update())
                    {
                        rfg_init_stage_1_done();
                        rfg_init_stage_2_prepare();
                        pThis->m_rfgGameInitializationStep++;
                    }
                    else
                    {
                        rfg_init_stage_1_loop_render(pGraphicsCommandWriter);
                    }
                    break;
                case 2:
                    if (rfg_init_stage_2_loop_update())
                    {
                        pThis->m_rfgGameInitializeThreadDone = false;
                        // Magic number: a 64 KB stack.
                        pThis->m_rfgGameInitializeThread.create("rfg_init_stage_2_done Thread", rfgGameInitializationAsyncThread, 0, 0x10000, 0);
                        pThis->m_rfgGameInitializeThread.start(pThis);
                        pThis->m_rfgGameInitializationStep++;
                    }
                    else
                    {
                        rfg_init_stage_2_loop_render(pGraphicsCommandWriter);
                    }
                    break;
                case 3:
                    if (!m_rfgGameInitializeThreadDone)
                    {
                        rfg_init_stage_2_loop_render(pGraphicsCommandWriter);
                        break;
                    }
                    pThis->m_rfgGameInitializeThread.join((uint32)Thread::TimeOutInfinite);
                    pThis->m_rfgGameInitializeThread.destroy();
                    // The pools come back from the boot thread.
                    packfile_set_thread_ownership();
                    stream2_set_thread_ownership();
                    havok_bpo_set_thread_ownership();
                    Permanent_mempool.set_thread_ownership();
                    Common_mempool.set_thread_ownership();
                    Interface_mempool.set_thread_ownership();
                    Interface_perm_mempool.set_thread_ownership();
                    Effect_preload_mempool.set_thread_ownership();
                    Material_effects_mempool.set_thread_ownership();
                    if (Table_preload_pool_ptr != nullptr)
                    {
                        Table_preload_pool_ptr->set_thread_ownership();
                    }
                    rfg_init_stage_2_done_2();
                    pThis->m_rfgGameInitializationStep++;
                    break;
                case 4:
                    pThis->m_rfgGameInitializationStep = 5;
                    // The simulation thread starts (updateSimulation's first wait).
                    pThis->m_simulationSignal.signal();
                    break;
                }
            }
            // Magic number: an expiring build ends after 200 frames.
            if (quit || Game_boot_exit_flag || gameseq_get_state() == GS_SHUTDOWN || (Expire_game != 0 && Framecount > 200))
            {
                GameFramework::quitGame(m_pGameframework);
                pThis->m_simulationThread.requestQuit();
                pThis->m_simulationSignal.signal();
                pThis->m_simulationThread.join((uint32)Thread::TimeOutInfinite);
            }
            rl_render_lib::get_instance()->next_frame();
        }

        // 0x00458BA0 (:644)
        // The second boot stage's thread: it owns the packfiles, stream2 and the boot pools while rfg_init_stage_2_done
        // runs, and reports when it is done (render step 3).
        int Application::rfgGameInitializationAsyncThread(const Thread& thread)
        {
            Application* pApplication = (Application*)thread.getArgument();
            packfile_set_thread_ownership();
            stream2_set_thread_ownership();
            Permanent_mempool.set_thread_ownership();
            Common_mempool.set_thread_ownership();
            Interface_mempool.set_thread_ownership();
            Interface_perm_mempool.set_thread_ownership();
            if (Table_preload_pool_ptr != nullptr)
            {
                Table_preload_pool_ptr->set_thread_ownership();
            }
            rfg_init_stage_2_done(pApplication->m_pSoundSystem);
            pApplication->m_rfgGameInitializeThreadDone = true;
            return 0;
        }

        // 0x004598C0 (:680)
        // The message box of a session interaction answered: OK picks the selected option, abort the last one, a forced
        // exit forgets the interaction.
        bool Application::handleGameSessionMsgboxResponse(int mbox_handle, int selection_index, msgbox_choice choice)
        {
            Application* pApplication = m_staticApplicationPointer;
            if (pApplication->m_gameSessionMsgboxHandle != mbox_handle)
            {
                return false;
            }
            if (choice == MBC_OK)
            {
                pApplication->m_gameSessionMsgboxSelection = selection_index;
                return true;
            }
            if (choice == MBC_ABORT)
            {
                pApplication->m_gameSessionMsgboxSelection = pApplication->m_gameSessionLastInteraction.responseOptionCount - 1;
                return true;
            }
            if (choice == MBC_FORCED_EXIT)
            {
                pApplication->m_gameSessionMsgboxSelection = 0xFFFFFFFF;
                memset(&pApplication->m_gameSessionLastInteraction, 0, sizeof(pApplication->m_gameSessionLastInteraction));
                pApplication->m_gameSessionMsgboxHandle = -1;
                return true;
            }
            return false;
        }

        // 0x00458C10 (:667)
        void Application::minimizeGameWindow(void* pArg)
        {
            if (pArg != nullptr)
            {
                GameFramework::minimizeGameWindow(((Application*)pArg)->m_pGameframework);
            }
        }

        // 0x00458C80 (:1206)
        void Application::shutdownRequested(GameFrameworkSystem*)
        {
        }

        // 0x00458C90 (:1211)
        bool Application::isReadyToShutdown(GameFrameworkSystem*)
        {
            return true;
        }

        // 0x00458CA0 (:1217)
        void Application::shutdown(GameFrameworkSystem* pGameFramework)
        {
            rfg_shutdown_game();
            SoundSystem::shutdown(m_pSoundSystem, GameFramework::getDefaultAllocator(pGameFramework));
            m_pSoundSystem = nullptr;
            m_localGameSession.destroy(GameFramework::getDefaultAllocator(pGameFramework));
            nvidia_highlights_shutdown();
            Discord_Shutdown();
        }

        // 0x0045A8B0 (:1162)
        // The simulation thread's half of the lockstep: wait for the main thread's frame, then the session work.
        void start_middle_of_frame()
        {
            Application& application = get_application();
            application.m_simulationSignal.sleepUntilSignaled();
            application.updateFromSimulationThread();
        }

        // 0x0045A920 (:1171)
        void end_middle_of_frame()
        {
            get_application().m_renderSignal.signal();
        }

        // 0x0045A960 (:1176)
        void lock_render()
        {
            get_application().m_renderMutex.lock(0);
            s_rendering_locked = true;
        }

        // 0x0045A9B0 (:1182)
        void unlock_render()
        {
            s_rendering_locked = false;
            get_application().m_renderMutex.unlock();
        }

        // 0x00458C60 (:1188)
        bool is_render_locked()
        {
            return s_rendering_locked;
        }
    }
}
#endif // OPENRFG_APPLICATION_MAIN
