# OpenRFG (orig_main.md, new code): the original entry, keen GameFramework (OM2), keen::rfg::Application and the
# original boot (OM3: rfg_app, rfg_loop) on the stand-ins of src/stubs: the common, multi and render scopes, the
# framework scope (the framework's platform services) and the main scope (*_main.cpp by hand, *_main_auto.cpp generated:
# tools/headless/game_stubgen.py --split src/stubs _main_auto <link log>), and main_hooks.cpp. Since stub_impl.md S1
# every src/stubs file is OpenRFG's, ungated; what only the test programs link is in tests/stubs.
# OM5: the recovered 2D path, fonts, console and localization. OM6: the multiplayer command line
# (main_options.cpp), with the original lobby's calls in mp_lobby_calls.cpp.
# OpenRFG runs the original UI (rfg_ui; UI.md UI3-UI6), its command line driving the original menus through their
# seams (main_ui_command_line.cpp).
# The test programs that lived beside it in tests/main (om1-om7) were deleted (user requests 2026-10-29 and 2026-11-04).
set(MAIN_SOURCES
    main_audio_log.cpp
    main_options.cpp
    mp_lobby_calls.cpp
    ${OPENRFG_STUBS_FRAMEWORK}
    ${OPENRFG_STUBS_MAIN}
    ${OPENRFG_STUBS_MULTI}
    ${OPENRFG_STUBS_RENDER}
    ${OPENRFG_STUBS_COMMON})

add_executable(OpenRFG WIN32 main_hooks.cpp main_game_trace.cpp main_ui_command_line.cpp ${MAIN_SOURCES})
# The order matters. stub_impl.md S8: the vehicles. UI.md UI3: the original UI (rfg_ui, public OPENRFG_UI).
# VFX.md 2.6, SFX.md 2.6: the effect and audio systems before rfg_render. stub_impl.md S2: NVIDIA
# Highlights.
target_link_libraries(OpenRFG PRIVATE
    rfg_app
    rfg_loop
    rfg_vehicle
    rfg_gameplay
    rfg_ai
    rfg_ui
    rfg_multi
    rfg_effects
    rfg_audio
    rfg_render
    keen_linkedresources
    dbghelp
    rfg_nvidia_highlights)
target_compile_definitions(OpenRFG PRIVATE OPENRFG_MULTI_CORE)
# rfg.exe is large address aware (its memory layout takes about 2.3 GB, orig_main.md 2.8).
target_link_options(OpenRFG PRIVATE /LARGEADDRESSAWARE)
