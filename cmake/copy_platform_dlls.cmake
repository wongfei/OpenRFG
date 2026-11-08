# Copies the platform DLLs a program links (stub_impl.md S2, 4.1) next to it:
#     cmake -DDLLS=<a|b|...> -DSTEAM_API=<steam_api.dll> -DDEST=<dir> -P copy_platform_dlls.cmake
# DLLS is $<TARGET_RUNTIME_DLLS:...> with "|" separators (may be empty). steam_api.dll goes along with sw_api.dll, which
# loads it at run time instead of importing it, and so does thqnocfg_steam.dat (next to steam_api.dll in the game
# directory): sw_api.dll's SteamAPI_Init stops the process (0xC0000409) when it is missing next to the program.
# The game directory is optional for the build: a DLL it does not have is reported and skipped, and the program then
# needs that DLL put next to it by hand before it can start.
string(REPLACE "|" ";" DLLS "${DLLS}")
foreach(dll ${DLLS})
    get_filename_component(dll_name ${dll} NAME)
    if(NOT EXISTS ${dll})
        message(STATUS "Not copied, no such file: ${dll}")
        continue()
    endif()
    file(COPY_FILE ${dll} ${DEST}/${dll_name} ONLY_IF_DIFFERENT)
    if(dll_name STREQUAL "sw_api.dll")
        if(EXISTS ${STEAM_API})
            file(COPY_FILE ${STEAM_API} ${DEST}/steam_api.dll ONLY_IF_DIFFERENT)
        endif()
        get_filename_component(game_dir ${STEAM_API} DIRECTORY)
        if(EXISTS ${game_dir}/thqnocfg_steam.dat)
            file(COPY_FILE ${game_dir}/thqnocfg_steam.dat ${DEST}/thqnocfg_steam.dat ONLY_IF_DIFFERENT)
        endif()
    endif()
endforeach()
