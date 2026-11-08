// Original: game/sources/application.hpp
// Implementation: game/sources/application.cpp
//
// Glue between the keen game framework and the Volition engine. keen drives it from the main thread;
// the Volition simulation runs on m_simulationThread in lockstep with render() (decompiled behaviour):
//   main (render, step 5):  rfg_set_simulationThreadCommandWriter(writer); m_simulationSignal.signal();
//                           m_renderSignal.sleepUntilSignaled(); lock m_renderMutex; writer = null;
//                           do_game_render_frame(writer); unlock
//   sim (updateSimulation): wait m_simulationSignal once, claim pool/thread ownership, then loop
//                           do_game_main_loop_frame until m_quitRequested. Inside each frame
//                           game_handshake_do_frame calls start_middle_of_frame (wait m_simulationSignal,
//                           updateFromSimulationThread) and end_middle_of_frame (signal m_renderSignal).
// Steps 0-4 of m_rfgGameInitializationStep run the staged Volition init from render(): rfg_pre_initialize_game,
// rfg_init_stage_1_*, rfg_init_stage_2_*, rfg_init_stage_2_done on m_rfgGameInitializeThread, then
// rfg_init_stage_2_done_2; step 4 sets 5 and releases the simulation thread.
#pragma once

#include "keen/base/event.hpp"
#include "keen/base/mutex.hpp"
#include "keen/base/thread.hpp"
#include "keen/base/types.hpp"
#include "keen/gameframework/gameapplication.hpp"
#include "keen/input/inputevents.hpp"
#include "keen/sound/soundsystem.hpp"
#include "unit4/gamesession/localgamesession.hpp"
#include "volition/rfg/code/interface/ui_message_box.h"

namespace keen
{
    struct GameFrameworkParameters;
    struct GameFrameworkSystem;
    struct UserAccountSystem;
    struct InputSystem;

    namespace rfg
    {
        // The singleton returned by keen::getGameApplication() (function-local static s_appInstance).
        // size 0x690
        class Application : public GameApplication
        {
        public:
            Application(); // 0x00459C80 (:115)
            virtual ~Application(); // vtbl+0x00 0x004597A0 (:122)
            virtual void fillGameframeworkParameters(GameFrameworkParameters* pParameters, const GameApplicationParameters& appParameters); // vtbl+0x04 0x00459D80 (:126)
            virtual void initialize(GameFrameworkSystem* pGameFramework, const GameApplicationParameters& parameters); // vtbl+0x08 0x00459F30 (:412)
            virtual void shutdownRequested(GameFrameworkSystem* pGameFramework); // vtbl+0x0C 0x00458C80 (:1206)
            virtual bool isReadyToShutdown(GameFrameworkSystem* pGameFramework); // vtbl+0x10 0x00458C90 (:1211)
            virtual void shutdown(GameFrameworkSystem* pGameFramework); // vtbl+0x14 0x00458CA0 (:1217)
            virtual void updateTime(float timeStep); // vtbl+0x18 0x0045A880 (:711)
            void updateFromSimulationThread(); // 0x0045A370 (:753)
            virtual void render(GraphicsCommandWriter* pGraphicsCommandWriter) const; // vtbl+0x1C 0x00459970 (:1038)
            virtual bool processInputEvent(const InputEvent& inputEvent); // vtbl+0x20 0x00458CF0 (:1467)

        private:
            void fillSavedataParameters(GameFrameworkParameters* pParameters); // 0x004594C0 (:1822)
            void fillOnlineSystemParameters(GameFrameworkParameters* pParameters); // 0x00459B90 (:1876)
            static void minimizeGameWindow(void* pArg); // 0x00458C10 (:667)
            static int rfgGameInitializationAsyncThread(const Thread& thread); // 0x00458BA0 (:644)
            static int rfgGameInitializationAsyncThread2(const Thread&); // declared, never emitted
            static bool handleGameSessionMsgboxResponse(int mbox_handle, int selection_index, msgbox_choice choice); // 0x004598C0 (:680)
            static int runSimulation(const Thread& thread); // 0x00459F20 (:372), calls updateSimulation on thread.getArgument()
            int updateSimulation(const Thread& thread); // 0x004597F0 (:377)

            uint32 m_rfgGameInitializationStep; // 0x08
            Thread m_rfgGameInitializeThread; // 0x0C
            bool m_rfgGameInitializeThreadDone; // 0x60
            unit4::LocalGameSession m_localGameSession; // 0x64
            UserAccountSystem* m_pUserAccountSystem; // 0xA4
            InputSystem* m_pInputSystem; // 0xA8
            unit4::SystemServices* m_pSystemServices; // 0xAC
            uint32 m_userAccountOperationHandle; // 0xB0
            char m_nativeGamebuildPath[1024]; // 0xB4
            GameFrameworkSystem* m_pGameframework; // 0x4B4
            SoundSystem::System* m_pSoundSystem; // 0x4B8
            int m_gameSessionMsgboxHandle; // 0x4BC
            unit4::SessionInteractionData m_gameSessionLastInteraction; // 0x4C0
            static Application* m_staticApplicationPointer; // 0x01A37844
            uint32 m_gameSessionMsgboxSelection; // 0x5F0
            Thread m_simulationThread; // 0x5F4
            Event m_simulationSignal; // 0x648
            Event m_renderSignal; // 0x64C
            volatile bool m_simulationDone; // 0x650
            bool m_hasDoneSimulationFrame; // 0x651
            Mutex m_renderMutex; // 0x654
            ControllerClass m_lastControllerClass; // 0x68C

            // The lockstep functions below reach the singleton's signals and mutex (application.cpp).
            friend void start_middle_of_frame();
            friend void end_middle_of_frame();
            friend void lock_render();
            friend void unlock_render();
        };

        // Sim-thread side of the lockstep, called from game_handshake_do_frame.
        void start_middle_of_frame(); // 0x0045A8B0 (:1162)
        void end_middle_of_frame(); // 0x0045A920 (:1171)
        // m_renderMutex plus the thread-local flag s_rendering_locked (lock_render is called from rfg_do_frame).
        void lock_render(); // 0x0045A960 (:1176)
        void unlock_render(); // 0x0045A9B0 (:1182)
        bool is_render_locked(); // 0x00458C60 (:1188)
    }
}

// Port deviation 1 (orig_main.md 2.5, stub_impl.md S2): GameFrameworkParameters::useSteam, which rfg.exe always sets.
// true (the default) is rfg.exe's Steam start-up, for which OpenRFG sets the Steam app id environment; --no-steam clears it.
extern bool g_openrfgUseSteam;

// Port deviation 4: where the save data goes. rfg.exe always uses Steam's remote storage, OpenRFG too by default. The local
// SaveDataProviderWin32Native (the same file format) in g_openrfgSaveDirectory (--save-dir; null: the default folder) is
// used instead with --no-steam, --no-steam-cloud-saves or --save-dir.
extern bool g_openrfgSteamCloudSaves;
extern const char* g_openrfgSaveDirectory;

// Port deviation 3 (orig_main.md section 3): OpenRFG's --window-pos X Y, the top left corner of the window's frame in
// physical desktop pixels. Off (rfg.exe's fixed position) unless g_openrfgWindowPosSet.
extern bool g_openrfgWindowPosSet;
extern int g_openrfgWindowPosX;
extern int g_openrfgWindowPosY;
