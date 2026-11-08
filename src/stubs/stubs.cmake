# The stand-ins OpenRFG shares with the test programs (src/stubs, new code; README.md). They are no library: each
# program lists the scopes it links and compiles them with its own defines. A module folder's files are named by scope:
# <stem>.cpp (and other <stem>_*.cpp) for every program on the headless stand-ins, <stem>_multi.cpp for the programs
# with the real multiplayer core, <stem>_main.cpp and <stem>_main_auto.cpp for the programs on the original boot,
# <stem>_framework.cpp for the programs on the original entry, <stem>_render.cpp for the programs with rfg_render;
# <stem> is the folder's name. Every file is compiled by OpenRFG, without gates (stub_impl.md S1); what only the test
# programs link is in tests/stubs (stubs.cmake there).
file(GLOB_RECURSE _stub_files CONFIGURE_DEPENDS ${OPENRFG_STUBS}/*.cpp)
set(OPENRFG_STUBS_COMMON)
set(OPENRFG_STUBS_MULTI)
set(OPENRFG_STUBS_MAIN)
set(OPENRFG_STUBS_FRAMEWORK)
set(OPENRFG_STUBS_RENDER)
foreach(_file ${_stub_files})
    get_filename_component(_dir ${_file} DIRECTORY)
    get_filename_component(_stem ${_dir} NAME)
    get_filename_component(_name ${_file} NAME_WE)
    if(_name STREQUAL "${_stem}_multi")
        list(APPEND OPENRFG_STUBS_MULTI ${_file})
    elseif(_name STREQUAL "${_stem}_main" OR _name STREQUAL "${_stem}_main_auto")
        list(APPEND OPENRFG_STUBS_MAIN ${_file})
    elseif(_name STREQUAL "${_stem}_framework")
        list(APPEND OPENRFG_STUBS_FRAMEWORK ${_file})
    elseif(_name STREQUAL "${_stem}_render")
        list(APPEND OPENRFG_STUBS_RENDER ${_file})
    else()
        list(APPEND OPENRFG_STUBS_COMMON ${_file})
    endif()
endforeach()
