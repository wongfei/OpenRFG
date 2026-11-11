# Recovered sources. Only translation units that are implemented are listed here; the other .cpp files under src/
# are recovery stubs (TU-private types and declarations) and are not compiled yet.

# keen modules are included as "keen/<module>/x.hpp" (and "unit4/...", "kaiko/...") from each module's include dir.
file(GLOB OPENRFG_RUNTIME_MODULES LIST_DIRECTORIES true ${OPENRFG_RUNTIME}/*)
set(OPENRFG_INCLUDE_DIRS ${OPENRFG_SRC})
foreach(module ${OPENRFG_RUNTIME_MODULES})
    if(IS_DIRECTORY ${module}/include)
        list(APPEND OPENRFG_INCLUDE_DIRS ${module}/include)
    endif()
endforeach()

# ---- third party code compiled into rfg.exe (recovered)
add_library(keen_tlsf STATIC ${OPENRFG_SRC}/library/modules/thirdparty/tlsf/tlsf.c)
target_include_directories(keen_tlsf PUBLIC ${OPENRFG_SRC})

# ---- the platform DLLs rfg.exe imports (stub_impl.md S2, decision 2, 4.1): sw_api.dll (emgiec's Steamworks wrapper,
# the only way rfg.exe reaches Steam; it loads steam_api.dll itself), discord-rpc.dll and GfeSDK.dll (NVIDIA
# Highlights). The DLLs are game files, so the programs link the pregenerated import libraries in lib/win32
# (generate_deps.md). The DLLs are copied next to every program that links them (openrfg_copy_platform_dlls in the root
# CMakeLists.txt) when OPENRFG_GAME_DIR has them; the build does not need them, the program does to start.
set(OPENRFG_GAME_DIR "C:/Program Files (x86)/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered" CACHE PATH "The game directory the DLLs are copied from after the link (optional for the build)")
set(OPENRFG_IMPLIB_DIR ${CMAKE_SOURCE_DIR}/lib/win32)

# OPENRFG_STUB_SWAPI builds platform_sw_api from the stand-in sw_api.cpp (no Steam: run with --no-steam) instead
# of linking sw_api.dll's import library; the programs then need neither sw_api.dll nor steam_api.dll.
option(OPENRFG_STUB_SWAPI "Build the stand-in sw_api.cpp instead of linking lib/win32/sw_api.lib (no Steam)" OFF)

# OPENRFG_STUB_BINK builds platform_binkw32 from the stand-in bink.cpp (every video is one black frame) instead of
# linking binkw32.dll's import library; the programs then do not need binkw32.dll.
option(OPENRFG_STUB_BINK "Build the stand-in bink.cpp instead of linking lib/win32/binkw32.lib (black videos)" OFF)

# OPENRFG_STUB_DISCORD builds platform_discord_rpc from the stand-in discord_rpc.cpp (no rich presence)
# instead of linking discord-rpc.dll's import library; the programs then do not need discord-rpc.dll.
option(OPENRFG_STUB_DISCORD "Build the stand-in discord_rpc.cpp instead of linking lib/win32/discord-rpc.lib (no Discord)" OFF)

# OPENRFG_STUB_GFESDK builds platform_GfeSDK from the stand-in gfesdk.cpp (no NVIDIA highlights) instead of
# linking GfeSDK.dll's import library; the programs then do not need GfeSDK.dll.
option(OPENRFG_STUB_GFESDK "Build the stand-in gfesdk.cpp instead of linking lib/win32/GfeSDK.lib (no NVIDIA highlights)" OFF)

set(OPENRFG_PLATFORM_DLLS)

if(OPENRFG_STUB_GFESDK)
    add_library(platform_GfeSDK STATIC ${OPENRFG_SRC}/stubs/thirdparty/gfesdk.cpp)
    target_include_directories(platform_GfeSDK PUBLIC ${OPENRFG_SRC})
else()
    list(APPEND OPENRFG_PLATFORM_DLLS GfeSDK)
endif()

if(OPENRFG_STUB_DISCORD)
    add_library(platform_discord_rpc STATIC ${OPENRFG_SRC}/stubs/thirdparty/discord_rpc.cpp)
    target_include_directories(platform_discord_rpc PUBLIC ${OPENRFG_SRC})
else()
    list(APPEND OPENRFG_PLATFORM_DLLS discord-rpc)
endif()

if(OPENRFG_STUB_SWAPI)
    add_library(platform_sw_api STATIC ${OPENRFG_SRC}/stubs/thirdparty/sw_api.cpp)
    target_include_directories(platform_sw_api PUBLIC ${OPENRFG_SRC})
else()
    list(APPEND OPENRFG_PLATFORM_DLLS sw_api)
endif()

if(OPENRFG_STUB_BINK)
    add_library(platform_binkw32 STATIC ${OPENRFG_SRC}/stubs/thirdparty/bink.cpp)
    target_include_directories(platform_binkw32 PUBLIC ${OPENRFG_SRC})
else()
    list(APPEND OPENRFG_PLATFORM_DLLS binkw32)
endif()

foreach(dll ${OPENRFG_PLATFORM_DLLS})
    string(REPLACE "-" "_" dll_target ${dll})
    add_library(platform_${dll_target} SHARED IMPORTED GLOBAL)
    set_target_properties(platform_${dll_target} PROPERTIES IMPORTED_IMPLIB ${OPENRFG_IMPLIB_DIR}/${dll}.lib IMPORTED_LOCATION ${OPENRFG_GAME_DIR}/${dll}.dll)
endforeach()

# sw_api.dll loads steam_api.dll at run time (it does not import it), so the copy takes it along.
set(OPENRFG_STEAM_API_DLL ${OPENRFG_GAME_DIR}/steam_api.dll CACHE INTERNAL "")

# ---- keen base (subset: threading primitives, strings, file system; orig_main.md OM1: the memory system and layout,
# ini files, language, frame number, profiling timer, string scanning and the parameterized formatter)
set(KEEN_BASE ${OPENRFG_RUNTIME}/base/sources)
add_library(keen_base STATIC
    ${KEEN_BASE}/global/aliaspathfiledevice.cpp
    ${KEEN_BASE}/global/apdepacker.cpp
    ${KEEN_BASE}/global/binaryreader.cpp
    ${KEEN_BASE}/global/calendartime.cpp
    ${KEEN_BASE}/global/crc32.cpp
    ${KEEN_BASE}/global/file.cpp
    ${KEEN_BASE}/global/filepath.cpp
    ${KEEN_BASE}/global/filereadstream.cpp
    ${KEEN_BASE}/global/filesystem.cpp
    ${KEEN_BASE}/global/float16.cpp
    ${KEEN_BASE}/global/floattostring.cpp
    ${KEEN_BASE}/global/formatparameterizedstring.cpp
    ${KEEN_BASE}/global/formatstring.cpp
    ${KEEN_BASE}/global/formatstringinput.cpp
    ${KEEN_BASE}/global/formattedstringwriter.cpp
    ${KEEN_BASE}/global/framenumber.cpp
    ${KEEN_BASE}/global/indexarray.cpp
    ${KEEN_BASE}/global/inifile.cpp
    ${KEEN_BASE}/global/inivariablelist.cpp
    ${KEEN_BASE}/global/inivariables.cpp
    ${KEEN_BASE}/global/internallist.cpp
    ${KEEN_BASE}/global/language.cpp
    ${KEEN_BASE}/global/memoryallocator.cpp
    ${KEEN_BASE}/global/memoryfiledevice.cpp
    ${KEEN_BASE}/global/memorysystem.cpp
    ${KEEN_BASE}/global/memorywritestream.cpp
    ${KEEN_BASE}/global/nativefiledevice.cpp
    ${KEEN_BASE}/global/pakfiledevice.cpp
    ${KEEN_BASE}/global/platformtype.cpp
    ${KEEN_BASE}/global/randomnumbergenerator.cpp
    ${KEEN_BASE}/global/readstream.cpp
    ${KEEN_BASE}/global/search.cpp
    ${KEEN_BASE}/global/string.cpp
    ${KEEN_BASE}/global/stringbuilder.cpp
    ${KEEN_BASE}/global/stringencoding.cpp
    ${KEEN_BASE}/global/stringscanner.cpp
    ${KEEN_BASE}/global/stringtokenizer.cpp
    ${KEEN_BASE}/global/tlsfallocator.cpp
    ${KEEN_BASE}/global/writestream.cpp
    ${KEEN_BASE}/win32/event_win32.cpp
    ${KEEN_BASE}/win32/memorylayout_win32.cpp
    ${KEEN_BASE}/win32/mutex_win32.cpp
    ${KEEN_BASE}/win32/nativefile_win32.cpp
    ${KEEN_BASE}/win32/profilingtimer_win32.cpp
    ${KEEN_BASE}/win32/semaphore_win32.cpp
    ${KEEN_BASE}/win32/systemtimer_win32.cpp
    ${KEEN_BASE}/win32/thread_win32.cpp
    ${KEEN_BASE}/win32/trace_win32.cpp
)
target_include_directories(keen_base PUBLIC ${OPENRFG_INCLUDE_DIRS})
target_link_libraries(keen_base PUBLIC keen_tlsf)

# ---- keen game framework: the task system and the game's generated memory layout definition (orig_main.md OM1), WinMain,
# GameFramework::main and the framework's services, the render effect system and the build strings (OM2). The Steam,
# user account, save data and system services systems are rfg_net's (stub_impl.md S2).
add_library(keen_gameframework STATIC
    ${OPENRFG_RUNTIME}/task/sources/global/taskqueue.cpp
    ${OPENRFG_RUNTIME}/task/sources/global/tasksystem.cpp
    ${OPENRFG_RUNTIME}/gameframework/sources/global/gameapplication.cpp
    ${OPENRFG_RUNTIME}/gameframework/sources/global/gameframework.cpp
    ${OPENRFG_RUNTIME}/gameframework/sources/global/gameframeworkmain.cpp
    ${OPENRFG_RUNTIME}/gameframework/sources/win32/backtraceuploader.cpp
    ${OPENRFG_RUNTIME}/gameframework/sources/win32/gameframework_win32.cpp
    ${OPENRFG_RUNTIME}/rendereffect/sources/global/rendereffectsystem.cpp
    ${OPENRFG_SRC}/game/build/win32/steam/master/generated_sources/memorylayoutdefinition.cpp
    ${OPENRFG_SRC}/game/build/win32/steam/master/generated_sources/version.cpp
)
# rfg_net (stub_impl.md S2): the platform start-up and the Steam, user account, save data and system services systems.
target_link_libraries(keen_gameframework PUBLIC keen_base keen_graphics keen_linkedresources keen_input_system keen_sound rfg_net dbghelp winmm)

# ---- keen graphics on D3D11 (render_minimal.md V0). Only OpenRFG links it, so the headless tests stay GPU-free.
set(KEEN_GRAPHICS ${OPENRFG_RUNTIME}/graphics/sources)
set(KEEN_GRAPHICSBASE ${OPENRFG_RUNTIME}/graphicsbase/sources)
set(SHADER_VARIANTS ${OPENRFG_SRC}/game/build/win32/steam/master/shader)
add_library(keen_graphics STATIC
    ${KEEN_GRAPHICSBASE}/global/color.cpp
    ${KEEN_GRAPHICSBASE}/global/pixelformat.cpp
    ${KEEN_GRAPHICSBASE}/global/vertexattribute.cpp
    ${KEEN_GRAPHICSBASE}/global/viewport.cpp
    ${KEEN_GRAPHICS}/d3d11/fragmentshader_d3d11.cpp
    ${KEEN_GRAPHICS}/d3d11/graphicscommandbuffer_d3d11.cpp
    ${KEEN_GRAPHICS}/d3d11/graphicssystem_d3d11.cpp
    ${KEEN_GRAPHICS}/d3d11/occlusionquery_d3d11.cpp
    ${KEEN_GRAPHICS}/d3d11/rendergeometry_d3d11.cpp
    ${KEEN_GRAPHICS}/d3d11/skinning_d3d11.cpp
    ${KEEN_GRAPHICS}/d3d11/texturedata_d3d11.cpp
    ${KEEN_GRAPHICS}/d3d11/vertexshader_d3d11.cpp
    ${KEEN_GRAPHICS}/global/downsampledepth.cpp
    ${KEEN_GRAPHICS}/global/graphicscommandwriter.cpp
    ${KEEN_GRAPHICS}/global/graphicsstateobjectcache.cpp
    ${KEEN_GRAPHICS}/global/graphicsstateobjectpool.cpp
    ${KEEN_GRAPHICS}/global/graphicssystem.cpp
    ${KEEN_GRAPHICS}/global/occlusionquery.cpp
    ${KEEN_GRAPHICS}/global/postprocessingfxaa.cpp
    ${KEEN_GRAPHICS}/global/rendergeometry.cpp
    ${KEEN_GRAPHICS}/global/shaderfactory.cpp
    ${KEEN_GRAPHICS}/global/stockobjects.cpp
    ${KEEN_GRAPHICS}/global/texturedata.cpp
    ${KEEN_GRAPHICS}/global/vertexformat.cpp
    ${SHADER_VARIANTS}/copydepth_sv.cpp
    ${SHADER_VARIANTS}/downsampledepth_sv.cpp
    ${SHADER_VARIANTS}/fxaashader_sv.cpp
    ${SHADER_VARIANTS}/gaussblur_sv.cpp
    ${SHADER_VARIANTS}/ssaoblur_sv.cpp
    ${SHADER_VARIANTS}/ssaocalc_sv.cpp
    ${SHADER_VARIANTS}/ssaominify_sv.cpp
)
target_link_libraries(keen_graphics PUBLIC keen_base d3d11 dxgi d3dcompiler dxguid)

# ---- keen linked resources: the stock shader pak linked into rfg.exe, pregenerated in src/generated (generate_deps.md).
add_library(keen_linkedresources STATIC
    ${OPENRFG_RUNTIME}/linkedresources/source/linkedresources.cpp
    ${OPENRFG_SRC}/generated/linkedresources_data.cpp
)
target_link_libraries(keen_linkedresources PUBLIC keen_base)

# ---- keen math and resources, the parts keen sound needs (SFX.md SX1): Matrix43::Unit and the general inverse, the
# Resource base and ResourceFactory's own members. The rest of both modules is not ported.
add_library(keen_math STATIC ${OPENRFG_RUNTIME}/math/sources/global/matrix43.cpp)
target_link_libraries(keen_math PUBLIC keen_base)
add_library(keen_resources STATIC
    ${OPENRFG_RUNTIME}/resources/sources/global/resource.cpp
    ${OPENRFG_RUNTIME}/resources/sources/global/resourcefactory.cpp
)
target_link_libraries(keen_resources PUBLIC keen_base)

# ---- keen sound (SFX.md 2.6): the SoundSystem on XAudio2 2.7 and the null provider, sound_unity in rfg.exe. The
# DirectX June 2010 include directory holds the recovered 2.7 xaudio2.h/xaudio2fx.h (SFX.md 2.4); it is public and
# comes before the Windows SDK's 2.9 headers for every TU that includes <xaudio2.h>. Ported whole in SX1. OpenRFG links
# rfg_audio (OPENRFG_AUDIO) and uses the real SoundSystem::initialize/update/shutdown.
set(KEEN_SOUND ${OPENRFG_RUNTIME}/sound)
add_library(keen_sound STATIC
    ${KEEN_SOUND}/soundformat/sources/global/soundfileheader.cpp
    ${KEEN_SOUND}/soundformat/sources/global/soundfileheader_vov.cpp
    ${KEEN_SOUND}/sources/global/sound.cpp
    ${KEEN_SOUND}/sources/global/soundbank.cpp
    ${KEEN_SOUND}/sources/global/soundbankfactory.cpp
    ${KEEN_SOUND}/sources/global/sounddefinition.cpp
    ${KEEN_SOUND}/sources/global/sounddefinitionfactory.cpp
    ${KEEN_SOUND}/sources/global/soundfile.cpp
    ${KEEN_SOUND}/sources/global/soundfilefactory.cpp
    ${KEEN_SOUND}/sources/global/soundprovidernull.cpp
    ${KEEN_SOUND}/sources/global/soundsystem.cpp
    ${KEEN_SOUND}/sources/global/soundvolume.cpp
    ${KEEN_SOUND}/sources/global/temporarysounddefinition.cpp
    ${KEEN_SOUND}/sources/win32/soundfiledata_win32.cpp
    ${KEEN_SOUND}/sources/win32/soundsystem_win32.cpp
    ${KEEN_SOUND}/sources/xaudio2/soundproviderwave_xaudio2.cpp
    ${KEEN_SOUND}/sources/xaudio2/soundsystem_xaudio2.cpp
    ${KEEN_SOUND}/sources/xaudio2/soundvoice_xaudio2.cpp
)
target_include_directories(keen_sound BEFORE PUBLIC ${OPENRFG_SRC}/library/modules/thirdparty/platform/win32/modules/directx/directx_june10/include)
target_link_libraries(keen_sound PUBLIC keen_base keen_math keen_resources ole32)

# ---- keen input (subset: the win32 window procedure and the mouse cursor). The win32 message translation into input
# events, keen::generateInputEventForWin32Message, is the program's (tests/headless/headless_input.cpp).
add_library(keen_input STATIC ${OPENRFG_RUNTIME}/input/sources/win32/inputsystem_win32.cpp)
target_link_libraries(keen_input PUBLIC keen_base)

# ---- keen input system (orig_main.md OM2): the event queue, the controllers (XInput, DirectInput, Steam) and
# the win32 message translation. inputsystem_win32.cpp is compiled again with only the part keen_input leaves out
# (OPENRFG_INPUT_SYSTEM), so programs that link both get each function once; the test programs keep keen_input alone
# with their own message translation.
set(KEEN_INPUT ${OPENRFG_RUNTIME}/input/sources)
add_library(keen_input_system STATIC
    ${KEEN_INPUT}/global/gesturehelper.cpp
    ${KEEN_INPUT}/global/inputsystem.cpp
    ${KEEN_INPUT}/steam/controllersteam.cpp
    ${KEEN_INPUT}/win32/controllerdinput_win32.cpp
    ${KEEN_INPUT}/win32/inputsystem_win32.cpp
    ${KEEN_INPUT}/win32/isdinputdevicexinput.cpp
    ${KEEN_INPUT}/xinput/controller_xinput.cpp
)
target_compile_definitions(keen_input_system PRIVATE OPENRFG_INPUT_SYSTEM DIRECTINPUT_VERSION=0x0800)
target_link_libraries(keen_input_system PUBLIC keen_input platform_sw_api xinput dinput8 dxguid wbemuuid ole32 oleaut32)

# ---- Volition vlib (subset: memory pools and managers, threads, strings, crc, file IO)
set(VLIB ${OPENRFG_SRC}/volition/vlib/code)
add_library(vlib STATIC
    ${VLIB}/cfile/cfile.cpp
    ${VLIB}/cfile/keen_cfile.cpp
    ${VLIB}/cfile/packfile.cpp
    ${VLIB}/cfile/pc_cfile.cpp
    ${VLIB}/cfile/vdir.cpp
    ${VLIB}/cfile/vdir_internal.cpp
    ${VLIB}/bmpman/peg_assemble.cpp
    ${VLIB}/input/descent_physics.cpp
    ${VLIB}/math/bbox.cpp
    ${VLIB}/math/bbox2d.cpp
    ${VLIB}/math/crc.cpp
    ${VLIB}/math/floating.cpp
    ${VLIB}/math/md5.cpp
    ${VLIB}/math/fp_aabb.cpp
    ${VLIB}/math/ik_2link.cpp
    ${VLIB}/math/ix.cpp
    ${VLIB}/math/matrix.cpp
    ${VLIB}/math/matrix_interpolate.cpp
    ${VLIB}/math/rand.cpp
    ${VLIB}/math/spline.cpp
    ${VLIB}/math/plane.cpp
    ${VLIB}/math/polyclip.cpp
    # VFX.md FX0: stub (file statics only), ported with the glass in FX8
    ${VLIB}/math/triangulate.cpp
    ${VLIB}/math/vlib_vector2.cpp
    ${VLIB}/math/vlib_quaternion.cpp
    ${VLIB}/math/vlib_vector3.cpp
    ${VLIB}/math/vlib_vector4.cpp
    ${VLIB}/memory/mempool_base.cpp
    ${VLIB}/memory/page_mempool.cpp
    ${VLIB}/memory/page_pool.cpp
    ${VLIB}/memory/rfg_split_mempool.cpp
    ${VLIB}/memory/split_memblk.cpp
    ${VLIB}/memory/split_memmgr.cpp
    ${VLIB}/memory/split_mempool.cpp
    ${VLIB}/memory/split_page_mempool.cpp
    ${VLIB}/memory/async_mempool.cpp
    ${VLIB}/memory/dynamic_mempool.cpp
    ${VLIB}/memory/static_mempool.cpp
    ${VLIB}/memory/virtual_mempool.cpp
    ${VLIB}/memory/vmemory.cpp
    ${VLIB}/os/cmdline.cpp
    ${VLIB}/os/cs_wrapper.cpp
    ${VLIB}/os/lockfree_queues.cpp
    ${VLIB}/os/mutex.cpp
    ${VLIB}/os/os.cpp
    ${VLIB}/os/perfmon.cpp
    ${VLIB}/os/sync_event.cpp
    ${VLIB}/os/thread.cpp
    ${VLIB}/os/timer.cpp
    ${VLIB}/os/vconsole.cpp
    ${VLIB}/parse/parse.cpp
    ${VLIB}/parse/xml_table_tools.cpp
    ${VLIB}/time/timestamp.cpp
    ${VLIB}/util/bitarray.cpp
    ${VLIB}/util/fastxml.cpp
    ${VLIB}/util/xml_util.cpp
    ${VLIB}/util/hash_table.cpp
    ${VLIB}/util/rule_file.cpp
    ${VLIB}/util/string_comp.cpp
    ${VLIB}/util/string_pool.cpp
    ${VLIB}/util/string_util.cpp
    ${VLIB}/util/unicode_tools.cpp
    ${VLIB}/util/vfont.cpp
    ${VLIB}/util/zlib_vlib.cpp
    ${VLIB}/vlib_core/vlib_core.cpp
    # P6 (player_minimal.md): input; the original's vlib unity TU, with the input files only (they share statics)
    ${OPENRFG_SRC}/game/build/win32/steam/master/unity_sources/vlib_unity.cpp
)
# stub_impl.md S3: wtsapi32 for os.cpp (the session notifications of the vlib window)
target_link_libraries(vlib PUBLIC keen_base keen_input openrfg_zlib wtsapi32)
target_compile_definitions(vlib PUBLIC OPENRFG_EXTERNAL_ZLIB)

# ---- Volition stream2 (streaming system)
set(STREAM2 ${OPENRFG_SRC}/volition/stream2/code)
add_library(stream2 STATIC
    ${STREAM2}/stream2.cpp
    ${STREAM2}/stream2_allocator.cpp
    ${STREAM2}/stream2_container.cpp
    ${STREAM2}/stream2_os.cpp
    ${STREAM2}/stream2_prim.cpp
)
target_link_libraries(stream2 PUBLIC vlib)

# ---- Havok 2014.2.5, reimplemented from rfg.exe (havok_impl.md). One .cpp per original source file; the list grows
# per step (HK1-HK9). Functions not implemented yet are stand-ins in src/stubs/havok: when a
# function lands here, its stand-in is deleted (MSVC rejects the duplicate definition).
set(HAVOK ${OPENRFG_SRC}/volition/libmodules/havok/hk2014_2_5_r1/source)
add_library(havok STATIC
    # HK1: base (memory system, containers, objects, errors, base types)
    ${HAVOK}/common/base/algorithm/sort/hkradixsort.cpp
    ${HAVOK}/common/base/algorithm/unionfind/hkunionfind.cpp
    ${HAVOK}/common/base/container/array/hkarrayutil.cpp
    ${HAVOK}/common/base/container/array/hksmallarray.cpp
    ${HAVOK}/common/base/container/hkcontainerallocators.cpp
    ${HAVOK}/common/base/container/string/hkstring.cpp
    ${HAVOK}/common/base/container/string/hkstringbuf.cpp
    ${HAVOK}/common/base/container/string/hkstringptr.cpp
    ${HAVOK}/common/base/container/substring/hksubstring.cpp
    ${HAVOK}/common/base/debugutil/memoryexceptiontestingutil/hkmemoryexceptiontestingutil.cpp
    ${HAVOK}/common/base/debugutil/multithreadcheck/hkmultithreadcheck.cpp
    ${HAVOK}/common/base/memory/allocator/freelist/hkfixedsizeallocator.cpp
    ${HAVOK}/common/base/memory/allocator/freelist/hkfreelist.cpp
    ${HAVOK}/common/base/memory/allocator/freelist/hkfreelistallocator.cpp
    ${HAVOK}/common/base/memory/allocator/hkmemoryallocator.cpp
    ${HAVOK}/common/base/memory/allocator/largeblock/hklargeblockallocator.cpp
    ${HAVOK}/common/base/memory/allocator/lifo/hklifoallocator.cpp
    ${HAVOK}/common/base/memory/allocator/recall/hkrecallallocator.cpp
    ${HAVOK}/common/base/memory/allocator/solver/hksolverallocator.cpp
    ${HAVOK}/common/base/memory/allocator/thread/hkthreadmemory.cpp
    ${HAVOK}/common/base/memory/router/hkmemoryrouter.cpp
    ${HAVOK}/common/base/memory/system/freelist/hkfreelistmemorysystem.cpp
    ${HAVOK}/common/base/memory/system/hkmemorysystem.cpp
    ${HAVOK}/common/base/memory/util/hkmemutil.cpp
    ${HAVOK}/common/base/object/hkreferencedobject.cpp
    ${HAVOK}/common/base/object/hksingleton.cpp
    # stub_impl.md S4 SB: reflection
    ${HAVOK}/common/base/reflection/hkclass.cpp
    ${HAVOK}/common/base/reflection/hkclassenum.cpp
    ${HAVOK}/common/base/reflection/hkclassmember.cpp
    ${HAVOK}/common/base/reflection/hkclassmemberaccessor.cpp
    ${HAVOK}/common/base/reflection/hkclassupdate.cpp
    ${HAVOK}/common/base/reflection/hktypeinfo.cpp
    ${HAVOK}/common/base/reflection/registry/hkdefaultclassnameregistry.cpp
    ${HAVOK}/common/base/reflection/registry/hktypeinforegistry.cpp
    ${HAVOK}/common/base/reflection/registry/hkvtableclassregistry.cpp
    ${HAVOK}/common/base/reflection/util/hkvariantdatautil.cpp
    ${HAVOK}/common/base/system/error/hkdefaulterror.cpp
    ${HAVOK}/common/base/system/error/hkerror.cpp
    ${HAVOK}/common/base/system/io/iarchive/hkiarchive.cpp
    ${HAVOK}/common/base/system/io/istream/hkistream.cpp
    ${HAVOK}/common/base/system/io/oarchive/hkoarchive.cpp
    ${HAVOK}/common/base/system/io/ostream/hkostream.cpp
    ${HAVOK}/common/base/system/io/reader/hkstreamreader.cpp
    ${HAVOK}/common/base/system/io/reader/memory/hkmemorystreamreader.cpp
    ${HAVOK}/common/base/system/io/writer/array/hkarraystreamwriter.cpp
    ${HAVOK}/common/base/system/io/writer/buffered/hkbufferedstreamwriter.cpp
    ${HAVOK}/common/base/system/io/writer/hkstreamwriter.cpp
    ${HAVOK}/common/base/system/io/writer/offsetonly/hkoffsetonlystreamwriter.cpp
    ${HAVOK}/common/base/system/error/hkerrorfwd.cpp
    ${HAVOK}/common/base/system/hkbasesystem.cpp
    ${HAVOK}/common/base/thread/criticalsection/win32/hkwin32criticalsection.cpp
    ${HAVOK}/common/base/types/geometry/aabb/hkaabb.cpp
    ${HAVOK}/common/base/types/geometry/hkstridedvertices.cpp
    ${HAVOK}/common/base/types/hkbasetypes.cpp
    ${HAVOK}/common/base/types/hkufloat8.cpp
    ${HAVOK}/common/base/types/physics/contactpoint/hkcontactpoint.cpp
    ${HAVOK}/common/base/types/physics/contactpoint/hkcontactpointmaterial.cpp
    ${HAVOK}/common/base/types/properties/hksimpleproperty.cpp
    # stub_impl.md S4 SC: data model
    ${HAVOK}/common/serialize/data/dict/hkdataobjectdict.cpp
    ${HAVOK}/common/serialize/data/hkdataobject.cpp
    ${HAVOK}/common/serialize/data/hkdataobjectimpl.cpp
    ${HAVOK}/common/serialize/data/native/hkdataobjectnative.cpp
    ${HAVOK}/common/serialize/data/util/hkdataobjectutil.cpp
    ${HAVOK}/common/serialize/typemanager/hktypemanager.cpp
    # stub_impl.md S4 SD: tagfile load
    ${HAVOK}/common/serialize/data/util/hkdataobjecttonative.cpp
    ${HAVOK}/common/serialize/data/util/hkdataworldcloner.cpp
    ${HAVOK}/common/serialize/packfile/binary/hkbinarypackfilewriter.cpp
    ${HAVOK}/common/serialize/packfile/hkpackfiledata.cpp
    ${HAVOK}/common/serialize/resource/hkobjectresource.cpp
    ${HAVOK}/common/serialize/tagfile/binary/hkbinarytagfilereader.cpp
    ${HAVOK}/common/serialize/tagfile/binary/hktagstringlist.cpp
    ${HAVOK}/common/serialize/tagfile/hktagfilereader.cpp
    ${HAVOK}/common/serialize/util/hkbuiltintyperegistry.cpp
    ${HAVOK}/common/serialize/util/hkchainedclassnameregistry.cpp
    ${HAVOK}/common/serialize/util/hkserializedeprecated.cpp
    ${HAVOK}/common/serialize/util/hkserializeutil.cpp
    ${HAVOK}/common/serialize/util/hkstaticclassnameregistry.cpp
    ${HAVOK}/common/serialize/util/hkstructurelayout.cpp
    ${HAVOK}/common/serialize/version/hkversionpatchmanager.cpp
    ${HAVOK}/common/serialize/version/hkversionutil.cpp
    # stub_impl.md S4 SE: packfiles, copier
    ${HAVOK}/common/serialize/copier/hkdeepcopier.cpp
    ${HAVOK}/common/serialize/copier/hkobjectcopier.cpp
    ${HAVOK}/common/serialize/packfile/binary/hkpackfileheader.cpp
    ${HAVOK}/common/serialize/packfile/binary/hkpackfilesectionheader.cpp
    ${HAVOK}/common/serialize/packfile/hkpackfilewriter.cpp
    ${HAVOK}/common/serialize/serialize/hkrelocationinfo.cpp
    ${HAVOK}/common/serialize/serialize/platform/hkplatformobjectwriter.cpp
    ${HAVOK}/common/serialize/util/hkclasspointervtable.cpp
    ${HAVOK}/common/serialize/util/hknativepackfileutils.cpp
    ${HAVOK}/common/serialize/util/hkobjectinspector.cpp
    ${HAVOK}/physics2012/dynamics/world/memory/hkpworldmemoryavailablewatchdog.cpp
    # stub_impl.md S4 SF: XML
    ${HAVOK}/common/serialize/serialize/xml/hkxmlobjectreader.cpp
    ${HAVOK}/common/serialize/serialize/xml/hkxmlobjectwriter.cpp
    ${HAVOK}/common/serialize/tagfile/xml/hkxmltagfilereader.cpp
    ${HAVOK}/common/serialize/util/xml/hkfloatparseutil.cpp
    ${HAVOK}/common/serialize/util/xml/hkparserbuffer.cpp
    ${HAVOK}/common/serialize/util/xml/hkxmllexanalyzer.cpp
    ${HAVOK}/common/serialize/util/xml/hkxmlparser.cpp
    ${HAVOK}/common/serialize/util/xml/hkxmlstreamparser.cpp
    # stub_impl.md S4 SG: versioning (the patch lists are globbed below)
    ${HAVOK}/common/base/algorithm/pseudorandom/hkmt19937randomgenerator.cpp
    ${HAVOK}/common/base/types/uuid/hkuuid.cpp
    # HK2: math (inline SSE code in the headers, common/base/math/hkmath.h)
    ${HAVOK}/common/base/algorithm/collide/linesegment/hklinesegmentutil.cpp
    ${HAVOK}/common/base/math/hkmath.cpp
    ${HAVOK}/common/base/math/matrix/hkmatrix3f.cpp
    ${HAVOK}/common/base/math/matrix/hkmatrix4f.cpp
    ${HAVOK}/common/base/math/matrix/hkmatrix6f.cpp
    ${HAVOK}/common/base/math/matrix/hkrotationf.cpp
    ${HAVOK}/common/base/math/matrix/hktransformf.cpp
    ${HAVOK}/common/base/math/qstransform/hkqstransformf.cpp
    ${HAVOK}/common/base/math/quaternion/hkquaternionf.cpp
    ${HAVOK}/common/base/math/swepttransform/hkswepttransformf.cpp
    ${HAVOK}/common/base/math/swepttransform/hkswepttransformutil.cpp
    ${HAVOK}/common/base/math/vector/hkfourtransposedpointsf.cpp
    ${HAVOK}/common/base/math/vector/hkvector4f.cpp
    ${HAVOK}/common/base/types/geometry/aabb/hkaabbutil.cpp
    ${HAVOK}/common/base/types/physics/motionstate/hkmotionstate.cpp
    # HK3: shapes and geometry
    ${HAVOK}/common/base/types/geometry/hkgeometry.cpp
    ${HAVOK}/common/geometryutilities/inertia/hkinertiatensorcomputer.cpp
    ${HAVOK}/common/internal/geometryprocessing/convexhull/hkgpconvexhull.cpp
    ${HAVOK}/common/internal/geometryprocessing/convexhull/hkgpconvexhullinternals.cpp
    ${HAVOK}/geometry/collide/shapes/hkcdshape.cpp
    ${HAVOK}/geometry/collide/algorithms/triangle/hkcdtriangleutil.cpp
    ${HAVOK}/geometry/internal/algorithms/gsk/hkcdgsk.cpp
    ${HAVOK}/geometry/internal/algorithms/raycast/hkcdraycasttriangle.cpp
    ${HAVOK}/physics2012/collide/query/collector/raycollector/hkpallrayhitcollector.cpp
    ${HAVOK}/physics2012/collide/query/collector/raycollector/hkpclosestrayhitcollector.cpp
    ${HAVOK}/physics2012/collide/shape/hkpshape.cpp
    ${HAVOK}/physics2012/collide/shape/compound/collection/extendedmeshshape/hkpextendedmeshshape.cpp
    ${HAVOK}/physics2012/collide/shape/compound/collection/hkpshapecollection.cpp
    ${HAVOK}/physics2012/collide/shape/compound/collection/list/hkplistshape.cpp
    ${HAVOK}/physics2012/collide/shape/compound/tree/hkpbvtreeshape.cpp
    ${HAVOK}/physics2012/collide/shape/compound/tree/mopp/hkpmoppbvtreeshape.cpp
    ${HAVOK}/physics2012/collide/shape/hkpshapebase.cpp
    ${HAVOK}/physics2012/collide/shape/hkpshapecontainer.cpp
    ${HAVOK}/physics2012/collide/shape/hkpshapetype.cpp
    ${HAVOK}/physics2012/collide/shape/query/hkpaabbcastcollector.cpp
    ${HAVOK}/physics2012/collide/shape/query/hkprayhitcollector.cpp
    ${HAVOK}/physics2012/collide/shape/convex/box/hkpboxshape.cpp
    ${HAVOK}/physics2012/collide/shape/convex/capsule/hkpcapsuleshape.cpp
    ${HAVOK}/physics2012/collide/shape/convex/convextransform/hkpconvextransformshape.cpp
    ${HAVOK}/physics2012/collide/shape/convex/convexvertices/hkpconvexverticesshape.cpp
    ${HAVOK}/physics2012/collide/shape/convex/cylinder/hkpcylindershape.cpp
    ${HAVOK}/physics2012/collide/shape/convex/convextranslate/hkpconvextranslateshape.cpp
    ${HAVOK}/physics2012/collide/shape/convex/hkpconvexshape.cpp
    ${HAVOK}/physics2012/collide/shape/convex/sphere/hkpsphereshape.cpp
    ${HAVOK}/physics2012/collide/shape/convex/triangle/hkptriangleshape.cpp
    ${HAVOK}/physics2012/collide/shape/heightfield/hkpheightfieldshape.cpp
    ${HAVOK}/physics2012/collide/shape/heightfield/sampledheightfield/hkpsampledheightfieldshape.cpp
    ${HAVOK}/physics2012/collide/shape/heightfield/sampledheightfield/hkpsampledheightfieldshape_default.cpp
    ${HAVOK}/physics2012/collide/shape/heightfield/sampledheightfield/hkpstoragesampledheightfieldshape.cpp
    ${HAVOK}/physics2012/collide/shape/heightfield/trisampledheightfield/hkptrisampledheightfieldbvtreeshape.cpp
    ${HAVOK}/physics2012/collide/shape/heightfield/trisampledheightfield/hkptrisampledheightfieldcollection.cpp
    ${HAVOK}/physics2012/collide/shape/misc/bv/hkpbvshape.cpp
    ${HAVOK}/physics2012/collide/shape/misc/phantomcallback/hkpphantomcallbackshape.cpp
    ${HAVOK}/physics2012/collide/shape/misc/transform/hkptransformshape.cpp
    ${HAVOK}/physics2012/collide/util/hkptriangleutil.cpp
    ${HAVOK}/physics2012/collide/util/shapedepth/hkpshapedepthutil.cpp
    ${HAVOK}/physics2012/collide/util/welding/hkpweldingutility.cpp
    ${HAVOK}/physics2012/internal/collide/mopp/code/hkpmoppcode.cpp
    ${HAVOK}/physics2012/internal/collide/mopp/machine/hkpmopplongrayvirtualmachine.cpp
    ${HAVOK}/physics2012/internal/collide/mopp/machine/hkpmoppaabbcastvirtualmachine.cpp
    ${HAVOK}/physics2012/internal/collide/mopp/machine/hkpmoppobbvirtualmachine.cpp
    ${HAVOK}/physics2012/internal/collide/mopp/machine/hkpmoppvirtualmachine.cpp
    ${HAVOK}/physics2012/utilities/dynamics/inertia/hkpaccurateinertiatensorcomputer.cpp
    ${HAVOK}/physics2012/utilities/dynamics/inertia/hkpinertiatensorcomputer.cpp
    # HK4: broadphase and filters
    ${HAVOK}/common/base/algorithm/collide/1axissweep/hk1axissweep.cpp
    ${HAVOK}/physics2012/collide/broadphase/3axissweep/hkp3axissweep.cpp
    ${HAVOK}/physics2012/collide/broadphase/hkpbroadphase.cpp
    ${HAVOK}/physics2012/collide/dispatch/broadphase/hkpnullbroadphaselistener.cpp
    ${HAVOK}/physics2012/collide/dispatch/broadphase/hkptypedbroadphasedispatcher.cpp
    ${HAVOK}/physics2012/collide/filter/defaultconvexlist/hkpdefaultconvexlistfilter.cpp
    ${HAVOK}/physics2012/collide/filter/hkpcollisionfilter.cpp
    ${HAVOK}/physics2012/collide/filter/null/hkpnullcollisionfilter.cpp
    ${HAVOK}/physics2012/dynamics/collide/filter/constraint/hkpconstraintcollisionfilter.cpp
    ${HAVOK}/physics2012/dynamics/collide/filter/pair/hkppaircollisionfilter.cpp
    ${HAVOK}/physics2012/dynamics/phantom/hkpphantombroadphaselistener.cpp
    ${HAVOK}/physics2012/dynamics/world/broadphaseborder/hkpbroadphaseborder.cpp
    ${HAVOK}/physics2012/dynamics/world/util/broadphase/hkpbroadphaseborderlistener.cpp
    ${HAVOK}/physics2012/dynamics/world/util/broadphase/hkpentityentitybroadphaselistener.cpp
    # HK5: world queries and collectors, optional components
    ${HAVOK}/common/base/config/hkoptionalcomponent.cpp
    ${HAVOK}/physics2012/collide/agent/query/hkpcdbodypaircollector.cpp
    ${HAVOK}/physics2012/collide/agent/query/hkpcdpointcollector.cpp
    ${HAVOK}/physics2012/collide/agent/util/symmetric/hkpsymmetricagentlinearcast.cpp
    ${HAVOK}/physics2012/collide/query/castutil/hkpsimpleworldraycaster.cpp
    ${HAVOK}/physics2012/collide/query/castutil/hkpworldlinearcaster.cpp
    ${HAVOK}/physics2012/collide/query/castutil/hkpworldraycaster.cpp
    ${HAVOK}/physics2012/collide/query/collector/bodypaircollector/hkpflagcdbodypaircollector.cpp
    ${HAVOK}/physics2012/collide/query/collector/pointcollector/hkpallcdpointcollector.cpp
    ${HAVOK}/physics2012/collide/query/collector/pointcollector/hkpclosestcdpointcollector.cpp
    ${HAVOK}/physics2012/collide/query/collector/pointcollector/hkpsimpleclosestcontactcollector.cpp
    ${HAVOK}/physics2012/dynamics/world/hkpworld.cpp
    ${HAVOK}/physics2012/dynamics/phantom/hkpsimpleshapephantom.cpp
    # HK6: narrowphase (dispatcher, agents, GSK, agent machines, contact managers)
    ${HAVOK}/physics2012/collide/agent/collidable/hkpcollidable.cpp
    ${HAVOK}/physics2012/collide/dispatch/broadphase/hkptypedbroadphasehandle.cpp
    ${HAVOK}/physics2012/collide/agent/hkpcollisionagent.cpp
    ${HAVOK}/physics2012/collide/agent/util/null/hkpnullagent.cpp
    ${HAVOK}/physics2012/collide/agent/util/linearcast/hkpiterativelinearcastagent.cpp
    ${HAVOK}/physics2012/collide/agent/convexagent/gjk/hkpgskbaseagent.cpp
    ${HAVOK}/physics2012/collide/agent/convexagent/gjk/hkpgskfagent.cpp
    ${HAVOK}/physics2012/collide/agent/convexagent/gjk/hkppredgskfagent.cpp
    ${HAVOK}/physics2012/collide/agent/convexagent/boxbox/hkpboxboxagent.cpp
    ${HAVOK}/physics2012/collide/agent/convexagent/spheresphere/hkpspheresphereagent.cpp
    ${HAVOK}/physics2012/collide/agent/convexagent/spherebox/hkpsphereboxagent.cpp
    ${HAVOK}/physics2012/collide/agent/convexagent/spherecapsule/hkpspherecapsuleagent.cpp
    ${HAVOK}/physics2012/collide/agent/convexagent/spheretriangle/hkpspheretriangleagent.cpp
    ${HAVOK}/physics2012/collide/agent/convexagent/capsulecapsule/hkpcapsulecapsuleagent.cpp
    ${HAVOK}/physics2012/collide/agent/miscagent/phantom/hkpphantomagent.cpp
    ${HAVOK}/physics2012/collide/agent/compoundagent/bvtree/hkpbvtreeagent.cpp
    ${HAVOK}/physics2012/collide/agent/compoundagent/bvtree/hkpmoppagent.cpp
    ${HAVOK}/physics2012/collide/agent/compoundagent/shapecollection/hkpshapecollectionagent.cpp
    ${HAVOK}/physics2012/collide/agent/miscagent/transform/hkptransformagent.cpp
    ${HAVOK}/physics2012/collide/agent/miscagent/bv/hkpbvagent.cpp
    ${HAVOK}/physics2012/collide/agent/compoundagent/bvtreestream/hkpbvtreestreamagent.cpp
    ${HAVOK}/physics2012/collide/agent/compoundagent/bvtreestream/hkpmoppbvtreestreamagent.cpp
    ${HAVOK}/physics2012/collide/dispatch/hkpagentregisterutil.cpp
    # registration only (the agents themselves come later in HK6)
    ${HAVOK}/physics2012/collide/agent/compoundagent/bvtree/hkpbvcompressedmeshagent.cpp
    ${HAVOK}/physics2012/collide/agent/compoundagent/bvtree/hkpstaticcompoundagent.cpp
    ${HAVOK}/physics2012/collide/agent/compoundagent/list/hkplistagent.cpp
    ${HAVOK}/physics2012/collide/agent/convexagent/capsuletriangle/hkpcapsuletriangleagent.cpp
    ${HAVOK}/physics2012/collide/agent/deprecated/convexlist/hkpconvexlistagent.cpp
    ${HAVOK}/physics2012/collide/agent/deprecated/multisphere/hkpmultisphereagent.cpp
    ${HAVOK}/physics2012/collide/agent/deprecated/multispheretriangle/hkpmultispheretriangleagent.cpp
    ${HAVOK}/physics2012/collide/agent3/bvtree3/hkpbvtreeagent3.cpp
    ${HAVOK}/physics2012/collide/agent3/capsuletriangle/hkpcapsuletriangleagent3.cpp
    ${HAVOK}/physics2012/collide/agent3/collection3/hkpcollectionagent3.cpp
    ${HAVOK}/physics2012/collide/agent3/collectioncollection3/hkpcollectioncollectionagent3.cpp
    ${HAVOK}/physics2012/collide/agent3/convexlist3/hkpconvexlistagent3.cpp
    ${HAVOK}/physics2012/collide/agent3/list3/hkplistagent3.cpp
    ${HAVOK}/physics2012/collide/agent3/predgskcylinderagent3/hkppredgskcylinderagent3.cpp
    ${HAVOK}/physics2012/collide/agent3/predgskagent3/hkppredgskagent3.cpp
    ${HAVOK}/physics2012/collide/agent3/boxbox/hkpboxboxagent3.cpp
    ${HAVOK}/physics2012/collide/agent3/machine/1n/hkpagent1nmachine.cpp
    ${HAVOK}/physics2012/collide/agent3/machine/1n/hkpagent1nmachineprocess.cxx
    ${HAVOK}/physics2012/collide/agent3/machine/nn/hkpagentnnmachine.cpp
    ${HAVOK}/physics2012/collide/agent3/machine/nn/hkplinkedcollidable.cpp
    ${HAVOK}/physics2012/collide/boxbox/hkpboxboxcollisiondetection.cpp
    ${HAVOK}/physics2012/collide/boxbox/hkpboxboxmanifold.cpp
    ${HAVOK}/physics2012/collide/dispatch/agent3bridge/hkpagent3bridge.cpp
    ${HAVOK}/physics2012/collide/dispatch/hkpcollisiondispatcher.cpp
    ${HAVOK}/geometry/internal/types/hkcdwingededge.cpp
    ${HAVOK}/physics2012/collide/util/hkpcollidecapsuleutil.cpp
    ${HAVOK}/physics2012/collide/util/hkpcollidetriangleutil.cpp
    ${HAVOK}/physics2012/internal/collide/gjk/hkpgsk.cpp
    ${HAVOK}/physics2012/internal/collide/gjk/hkpgskcache.cpp
    ${HAVOK}/physics2012/internal/collide/gjk/agent/hkpgskagentutil.cpp
    ${HAVOK}/physics2012/internal/collide/gjk/continuous/hkpcontinuousgsk.cpp
    ${HAVOK}/physics2012/internal/collide/gjk/gskmanifold/hkpgskmanifoldutil.cpp
    ${HAVOK}/physics2012/internal/collide/gjk/penetration/hkpconvexpenetrationutil.cpp
    ${HAVOK}/physics2012/internal/collide/gjk/penetration/hkpcvxcvxdistbyheuristicsampling.cpp
    # HK7: dynamics, world and simulation
    ${HAVOK}/physics2012/dynamics/motion/hkpmotion.cpp
    ${HAVOK}/physics2012/dynamics/motion/rigid/hkpboxmotion.cpp
    ${HAVOK}/physics2012/dynamics/motion/rigid/hkpspheremotion.cpp
    ${HAVOK}/physics2012/dynamics/motion/rigid/hkpcharactermotion.cpp
    ${HAVOK}/physics2012/dynamics/motion/rigid/hkpkeyframedrigidmotion.cpp
    ${HAVOK}/physics2012/dynamics/motion/rigid/hkpfixedrigidmotion.cpp
    ${HAVOK}/physics2012/dynamics/motion/rigid/thinboxmotion/hkpthinboxmotion.cpp
    ${HAVOK}/physics2012/dynamics/common/hkpmaterial.cpp
    ${HAVOK}/physics2012/dynamics/world/hkpworldobject.cpp
    ${HAVOK}/physics2012/dynamics/entity/hkpentity.cpp
    ${HAVOK}/physics2012/dynamics/entity/hkprigidbody.cpp
    ${HAVOK}/physics2012/dynamics/phantom/hkpphantom.cpp
    ${HAVOK}/physics2012/dynamics/phantom/hkpshapephantom.cpp
    ${HAVOK}/physics2012/dynamics/phantom/hkpaabbphantom.cpp
    ${HAVOK}/physics2012/dynamics/action/hkpaction.cpp
    ${HAVOK}/physics2012/dynamics/action/hkpunaryaction.cpp
    ${HAVOK}/physics2012/dynamics/world/util/hkpworldcallbackutil.cpp
    ${HAVOK}/physics2012/dynamics/entity/util/hkpentitycallbackutil.cpp
    ${HAVOK}/physics2012/dynamics/world/hkpsimulationisland.cpp
    ${HAVOK}/physics2012/dynamics/world/util/hkpworldoperationqueue.cpp
    ${HAVOK}/physics2012/dynamics/world/extensions/hkpworldextension.cpp
    ${HAVOK}/physics2012/dynamics/world/util/hkpworldmemoryutil.cpp
    ${HAVOK}/physics2012/dynamics/world/hkpworldcinfo.cpp
    ${HAVOK}/physics2012/dynamics/world/maintenance/default/hkpdefaultworldmaintenancemgr.cpp
    ${HAVOK}/physics2012/internal/dynamics/world/simulation/continuous/toiresourcemgr/default/hkpdefaulttoiresourcemgr.cpp
    ${HAVOK}/physics2012/dynamics/entity/util/hkpentityaabbutil.cpp
    ${HAVOK}/physics2012/utilities/dynamics/keyframe/hkpkeyframeutility.cpp
    ${HAVOK}/physics2012/dynamics/world/util/hkpworldconstraintutil.cpp
    ${HAVOK}/physics2012/dynamics/world/util/hkpworldagentutil.cpp
    ${HAVOK}/physics2012/dynamics/motion/util/hkprigidmotionutil.cpp
    ${HAVOK}/physics2012/dynamics/world/util/hkpworldoperationutil.cpp
    ${HAVOK}/physics2012/dynamics/world/simulation/hkpsimulation.cpp
    ${HAVOK}/physics2012/dynamics/world/simulation/continuous/hkpcontinuoussimulation.cpp
    ${HAVOK}/physics/constraintsolver/constraint/hkpconstraintqueryin.cpp
    ${HAVOK}/physics2012/collide/dispatch/contactmgr/hkpcontactmgrfactory.cpp
    ${HAVOK}/physics2012/collide/dispatch/contactmgr/hkpnullcontactmgr.cpp
    ${HAVOK}/physics2012/dynamics/collide/hkpresponsemodifier.cpp
    ${HAVOK}/physics2012/dynamics/constraint/hkpconstraintowner.cpp
    ${HAVOK}/physics2012/dynamics/constraint/util/hkpconstraintcallbackutil.cpp
    ${HAVOK}/physics2012/dynamics/constraint/hkpconstraintinstance.cpp
    ${HAVOK}/physics2012/dynamics/constraint/hkpconstraintlistener.cpp
    ${HAVOK}/physics2012/dynamics/world/listener/hkpworldpostsimulationlistener.cpp
    ${HAVOK}/physics2012/dynamics/world/listener/hkpworlddeletionlistener.cpp
    ${HAVOK}/physics2012/dynamics/phantom/hkpphantomoverlaplistener.cpp
    ${HAVOK}/physics/constraint/data/hkpconstraintdata.cpp
    ${HAVOK}/physics2012/collide/agent/query/hkplinearcastcollisioninput.cpp
    ${HAVOK}/physics/constraintsolver/solve/hkpsolverinfo.cpp
    # HK8: contact constraint, contact managers, constraint solver, TOI solver
    ${HAVOK}/physics2012/dynamics/collide/hkpdynamicscontactmgr.cpp
    ${HAVOK}/physics2012/dynamics/collide/hkpsimpleconstraintcontactmgr.cpp
    ${HAVOK}/physics2012/dynamics/collide/deprecated/hkpreportcontactmgr.cpp
    ${HAVOK}/physics2012/dynamics/constraint/atom/hkpsimplecontactconstraintatomutil.cpp
    ${HAVOK}/physics2012/dynamics/constraint/atom/hkpmodifierconstraintatom.cpp
    ${HAVOK}/physics2012/dynamics/constraint/contact/hkpsimplecontactconstraintdata.cpp
    ${HAVOK}/physics2012/dynamics/constraint/contact/hkpsimplecontactconstraintutil.cpp
    ${HAVOK}/physics2012/dynamics/constraint/setup/hkpconstraintsolversetup.cpp
    ${HAVOK}/physics2012/internal/solver/contact/hkpsimplecontactconstraintinfo.cpp
    ${HAVOK}/physics2012/internal/solver/atom/hkpbuildjacobianfromatoms.cpp
    ${HAVOK}/physics/constraintsolver/constraint/bilateral/hkp1dbilateralconstraintbuildjacobian.cpp
    ${HAVOK}/physics2012/internal/solver/solve/hkpsolver.cpp
    ${HAVOK}/physics2012/internal/solver/solve/hkpsolverexport.cpp
    ${HAVOK}/physics2012/internal/solver/solve/hkpsolvergettoivelocities.cpp
    ${HAVOK}/physics/constraintsolver/jacobian/hkpjacobianschema.cpp
    ${HAVOK}/physics/constraintsolver/accumulator/hkpvelocityaccumulator.cpp
    ${HAVOK}/physics2012/internal/solver/simpleconstraints/hkpsimpleconstraintutil.cpp
    ${HAVOK}/physics2012/dynamics/constraint/response/hkpsimplecollisionresponse.cpp
    # HK9: what was left of the stand-ins
    ${HAVOK}/physics2012/dynamics/collide/contactlistener/hkpcontactlistener.cpp
    ${HAVOK}/physics2012/dynamics/collide/contactlistener/hkpcontactpointevent.cpp
    ${HAVOK}/physics2012/dynamics/collide/deprecated/hkpcollisionlistener.cpp
    ${HAVOK}/physics2012/dynamics/world/listener/hkpworldpostcollidelistener.cpp
    ${HAVOK}/physics2012/dynamics/collide/contactlistener/util/hkpendofstepcallbackutil.cpp
    ${HAVOK}/physics2012/dynamics/collide/contactlistener/util/hkpcollisioncallbackutil.cpp
    # MP7 (multiplayer.md): the ragdoll joints
    ${HAVOK}/physics/constraint/data/ragdoll/hkpragdollconstraintdata.cpp
    ${HAVOK}/physics/constraint/data/limitedhinge/hkplimitedhingeconstraintdata.cpp
    ${HAVOK}/physics/constraint/data/ballandsocket/hkpballandsocketconstraintdata.cpp
    # stuck projectiles (projectile::attach): the prismatic constraint
    ${HAVOK}/physics/constraint/data/prismatic/hkpprismaticconstraintdata.cpp
    # constraint motors, breakable constraints (havok_constraint_add_motor, havok_constraint_create)
    ${HAVOK}/physics/constraint/motor/position/hkppositionconstraintmotor.cpp
    ${HAVOK}/physics/constraint/motor/velocity/hkpvelocityconstraintmotor.cpp
    ${HAVOK}/physics/constraintsolver/constraint/motor/hkpmotorconstraintinfo.cpp
    ${HAVOK}/physics/constraint/atom/bridge/hkpbridgeconstraintatom.cpp
    ${HAVOK}/physics/constraint/data/wrapper/hkpwrappedconstraintdata.cpp
    ${HAVOK}/physics2012/dynamics/constraint/breakable/hkpbreakableconstraintdata.cpp
    # P1 (player_minimal.md): the character proxy
    ${HAVOK}/common/internal/simplexsolver/hksimplexsolver.cpp
    ${HAVOK}/physics2012/dynamics/entity/hkpentitylistener.cpp
    ${HAVOK}/physics2012/dynamics/phantom/hkpphantomlistener.cpp
    ${HAVOK}/physics2012/dynamics/phantom/hkpcachingshapephantom.cpp
    ${HAVOK}/physics2012/utilities/charactercontrol/characterproxy/hkpcharacterproxy.cpp
    ${HAVOK}/physics2012/utilities/charactercontrol/statemachine/util/hkpcharactermovementutil.cpp
    # stub_impl.md S8: the vehicle kit under hk_custom_vehicle_*
    ${HAVOK}/physics/constraintsolver/vehiclefriction/hkpvehiclefrictionsolver.cpp
    ${HAVOK}/physics2012/vehicle/aerodynamics/default/hkpvehicledefaultaerodynamics.cpp
    ${HAVOK}/physics2012/vehicle/brake/default/hkpvehicledefaultbrake.cpp
    ${HAVOK}/physics2012/vehicle/driverinput/default/hkpvehicledefaultanalogdriverinput.cpp
    ${HAVOK}/physics2012/vehicle/engine/default/hkpvehicledefaultengine.cpp
    ${HAVOK}/physics2012/vehicle/hkpvehicledata.cpp
    ${HAVOK}/physics2012/vehicle/hkpvehicleinstance.cpp
    ${HAVOK}/physics2012/vehicle/simulation/default/hkpvehicledefaultsimulation.cpp
    ${HAVOK}/physics2012/vehicle/steering/default/hkpvehicledefaultsteering.cpp
    ${HAVOK}/physics2012/vehicle/suspension/default/hkpvehicledefaultsuspension.cpp
    ${HAVOK}/physics2012/vehicle/transmission/default/hkpvehicledefaulttransmission.cpp
    ${HAVOK}/physics2012/vehicle/tyremarks/hkptyremarksinfo.cpp
    ${HAVOK}/physics2012/vehicle/velocitydamper/default/hkpvehicledefaultvelocitydamper.cpp
    ${HAVOK}/physics2012/vehicle/wheelcollide/raycast/hkpvehicleraycastwheelcollide.cpp
    ${HAVOK}/physics2012/vehicle/wheelcollide/rejectchassislistener/hkprejectchassislistener.cpp
)
# stub_impl.md S4 SB: the reflection data files, generated from rfg.exe by tools/hdrgen/reflgen.py (every
# <module>/classes/reflections/*reflection.cpp and the two version 1 class descriptor files)
file(GLOB_RECURSE HAVOK_REFLECTIONS CONFIGURE_DEPENDS ${HAVOK}/*reflection.cpp)
list(FILTER HAVOK_REFLECTIONS INCLUDE REGEX "/classes/reflections/[^/]*\\.cpp$")
target_sources(havok PRIVATE ${HAVOK_REFLECTIONS}
    ${HAVOK}/common/base/reflection/hkclassversion1class.cpp
    ${HAVOK}/common/base/reflection/hkclassversion1paddedclass.cpp)
# stub_impl.md S4 SG: the class version patches (compat/patches/<version>/<product>patches_<version>.cpp, each
# expanding its generated .cxx list, tools/hdrgen/patchgen.py)
file(GLOB_RECURSE HAVOK_PATCHES CONFIGURE_DEPENDS ${HAVOK}/common/compat/patches/*.cpp)
target_sources(havok PRIVATE ${HAVOK_PATCHES})
target_include_directories(havok PUBLIC ${OPENRFG_SRC})

# ---- Volition render_lib and rfg game code (subset: the tables H0 loads, stream2 registration and layout)
set(RFG ${OPENRFG_SRC}/volition/rfg/code)
# Unity files of the original build (game/build/.../unity_sources): modules whose .cpp files share file statics.
set(UNITY ${OPENRFG_SRC}/game/build/win32/steam/master/unity_sources)
add_library(rfg STATIC
    # P2 (player_minimal.md): animation
    ${UNITY}/anim_lib_unity.cpp
    ${UNITY}/rfg_geom_unity.cpp
    ${UNITY}/rfg_destruction_unity.cpp
    ${UNITY}/rfg_havok_unity.cpp
    ${UNITY}/rfg_objects_unity.cpp
    # P3 (player_minimal.md): the weapon tables
    ${UNITY}/rfg_weapons_unity.cpp
    # P6: the input event writer, control and the camera
    ${OPENRFG_SRC}/game/sources/application.cpp
    ${UNITY}/rfg_control_unity.cpp
    ${OPENRFG_SRC}/volition/render_lib/code/rl_aabb.cpp
    ${OPENRFG_SRC}/volition/render_lib/code/rl_color.cpp
    ${OPENRFG_SRC}/volition/render_lib/code/rl_frustum.cpp
    ${OPENRFG_SRC}/volition/render_lib/code/rl_material_data.cpp
    ${OPENRFG_SRC}/volition/render_lib/code/rl_material_map.cpp
    ${OPENRFG_SRC}/volition/render_lib/code/rl_mesh_data.cpp
    # P4: static meshes (item meshes, tags and collision models; V5 had them in rfg_render for the sky). The render
    # objects they create are faked by headless_stubs_render.cpp in the headless build.
    ${RFG}/geom/static_mesh.cpp
    ${RFG}/geom/static_mesh_shared.cpp
    ${UNITY}/rfg_camera_unity.cpp
    ${RFG}/code_effects/explosion.cpp
    ${RFG}/code_effects/explosion_shared.cpp
    ${RFG}/effects/effects.cpp
    ${RFG}/effects/effects_shared.cpp
    # stub_impl.md S5: the game log (its events are inline in definitions_events/events.h)
    ${RFG}/event_logging/gamelog.cpp
    ${RFG}/game_render/game_render_task.cpp
    ${RFG}/game_render_terrain.cpp
    ${RFG}/gameplay/gameflow/gameflow_common.cpp
    ${RFG}/gameplay/object_district.cpp
    ${RFG}/gameplay/managed_layer_loading.cpp
    ${RFG}/gameplay/managed_layer_loading_shared.cpp
    # P4: the upgrade table and the queries the player needs
    ${RFG}/gameplay/upgrades.cpp
    ${RFG}/gamewide/asset_assembler.cpp
    # stub_impl.md S5: the DLC value overrides and the animation preload lists
    ${RFG}/gamewide/custom_dlc_data.cpp
    ${RFG}/gamewide/game_stream2_register.cpp
    ${RFG}/gamewide/preload.cpp
    ${RFG}/gamewide/tweak_table.cpp
    ${RFG}/geom/decal_info.cpp
    ${RFG}/geom/decal_info_shared.cpp
    ${RFG}/geom/mesh_tags.cpp
    ${RFG}/level/rfg_chunk_shared.cpp
    # stub_impl.md S3: the terrain header and the build path name lists
    ${RFG}/level/rfg_terrainx_shared.cpp
    ${RFG}/level/rfg_prop_block_shared.cpp
    ${RFG}/level/rfg_stream_layout_shared.cpp
    ${RFG}/level/rfg_terrainx.cpp
    ${RFG}/level/rfg_zonex.cpp
    ${RFG}/level/rfg_zonex_shared.cpp
    ${RFG}/level/territory_shared.cpp
    # lod.md L2: the undergrowth (zones, cells, placement, wind, destruction events, the update-render task; its clone
    # mesh and foliage mesh calls are faked by headless_stubs_render.cpp in the headless build) and the weather's wind
    ${RFG}/level/undergrowth.cpp
    ${RFG}/level/undergrowth_shared.cpp
    ${RFG}/weather/weather.cpp
    ${RFG}/weather/wind.cpp
    ${RFG}/level/world.cpp
    ${RFG}/level/world_restore.cpp
    ${RFG}/level/world_serialize.cpp
    ${RFG}/level/world_stream.cpp
)
target_link_libraries(rfg PUBLIC stream2 vlib havok)
# stub_impl.md S2: Volition's no-op event logger, whole (rfg.exe links logger_stubs.cpp).
target_sources(rfg PRIVATE ${OPENRFG_SRC}/volition/vlogger_lib/logger_stubs.cpp)
# stub_impl.md S3: the debug menu table (rfg/os), rfg/util (the always-loaded tables, the pregame split lock)
target_sources(rfg PRIVATE ${RFG}/os/dc_menu.cpp ${RFG}/util/always_loaded_shared.cpp ${RFG}/util/special_allocators.cpp)

# ---- NVIDIA Highlights (stub_impl.md S2): nvidia_highlights.cpp whole over GfeSDK.dll. Only OpenRFG links it.
add_library(rfg_nvidia_highlights STATIC ${RFG}/nvidia_highlights.cpp)
target_link_libraries(rfg_nvidia_highlights PUBLIC keen_base platform_GfeSDK)

# ---- multi_core_compression.cpp and the single player part of multi_game_destruction.cpp compiled alone (H3/MP0 subsets),
# for the programs without the multi unity TU (rfg_multi, which compiles both whole from multiplayer.md MP5 on).
add_library(rfg_multi_alone STATIC
    ${RFG}/multi/multi_core/global/multi_core_compression.cpp
    ${RFG}/multi/multi_game/multi_game_destruction.cpp
)
target_compile_definitions(rfg_multi_alone PRIVATE OPENRFG_MULTI_DESTRUCTION_ALONE)
target_link_libraries(rfg_multi_alone PUBLIC rfg)

# ---- render_lib on keen graphics (render_minimal.md V1 and later). Only OpenRFG links it, so the
# headless tests stay GPU-free; the render stand-ins they use are not part of it. The CPU-only render_lib files the
# simulation needs (rl_color, rl_material_data, ...) stay in rfg.
set(RENDER_LIB ${OPENRFG_SRC}/volition/render_lib/code)
# Game data tables the render code reads (the star catalog): src/generated/rfgdata_tables.cpp, pregenerated like the
# linked resources (generate_deps.md).
add_library(rfg_render STATIC
    # V2: bitmap manager (vlib, but only the renderer registers textures with it) and the stream2 Pegs callbacks
    ${VLIB}/bmpman/bmpman.cpp
    ${RFG}/rfg.cpp
    ${RENDER_LIB}/rl_batched_pass.cpp
    ${RENDER_LIB}/rl_cached_dyn_texture.cpp
    ${RENDER_LIB}/rl_callback_widget.cpp
    ${RENDER_LIB}/rl_camera.cpp
    ${RENDER_LIB}/rl_clb_projector.cpp
    ${RENDER_LIB}/rl_clone_mesh_instance.cpp
    ${RENDER_LIB}/rl_compiled_render_group.cpp
    ${RENDER_LIB}/rl_compound_mesh_instance.cpp
    ${RENDER_LIB}/rl_destroyable_model_instance.cpp
    ${RENDER_LIB}/rl_draw.cpp
    # orig_main.md OM5: the 2D drawing and the fonts
    ${RENDER_LIB}/gr/gr.cpp
    ${RENDER_LIB}/gr/gr_font.cpp
    # VFX.md FX5: the world-to-screen projection of gr_3d.cpp (the radial blur position)
    ${RENDER_LIB}/gr/gr_3d.cpp
    # stub_misc.md group 5: the two stripped debug draw calls of camera_free_render_debug
    ${RENDER_LIB}/gr/grd.cpp
    ${RENDER_LIB}/rl_dyn_texture.cpp
    ${RENDER_LIB}/rl_factory_rtti.cpp
    ${RENDER_LIB}/rl_fog_volume.cpp
    ${RENDER_LIB}/rl_fp_kdtree.cpp
    ${RENDER_LIB}/rl_gpu_visibility.cpp
    ${RENDER_LIB}/rl_handle.cpp
    ${RENDER_LIB}/rl_instance_stream.cpp
    ${RENDER_LIB}/rl_hdr_state.cpp
    ${RENDER_LIB}/rl_light.cpp
    ${RENDER_LIB}/rl_material.cpp
    ${RENDER_LIB}/rl_material_data_render.cpp
    ${RENDER_LIB}/rl_material_library.cpp
    ${RENDER_LIB}/rl_material_map_render.cpp
    ${RENDER_LIB}/rl_mesh.cpp
    ${RENDER_LIB}/rl_mesh_instance.cpp
    ${RENDER_LIB}/rl_mesh_recompile.cpp
    ${RENDER_LIB}/rl_meteor_shower.cpp
    ${RENDER_LIB}/rl_metrics.cpp
    ${RENDER_LIB}/rl_morph_weights.cpp
    # VFX.md FX7: the deformable mesh decals and the morph instance stream binding they use
    ${RENDER_LIB}/rl_deformable_mesh_instance.cpp
    ${RENDER_LIB}/rl_morph_instance.cpp
    # stub_impl.md S3: the morph data loader and the morph container
    ${RENDER_LIB}/gaussblur.cpp
    ${RENDER_LIB}/rl_morph_data.cpp
    ${RENDER_LIB}/rl_morph_target.cpp
    ${RENDER_LIB}/rl_occluder.cpp
    ${RENDER_LIB}/rl_partition_sector.cpp
    ${RENDER_LIB}/rl_partition_system.cpp
    ${RENDER_LIB}/rl_particle_renderer.cpp
    ${RENDER_LIB}/rl_primitive_renderer.cpp
    ${RENDER_LIB}/rl_primitive_state.cpp
    ${RENDER_LIB}/rl_quadtree.cpp
    ${RENDER_LIB}/rl_quadtree_sector.cpp
    ${RENDER_LIB}/rl_render_lib.cpp
    # bug_blood_decals.md (deviation, new code): load-time fix of the decal vertex shaders' normal decode
    ${RENDER_LIB}/rl_shader_fixup.cpp
    ${RENDER_LIB}/rl_render_texture.cpp
    ${RENDER_LIB}/rl_renderable_instance.cpp
    ${RENDER_LIB}/rl_renderer.cpp
    ${RENDER_LIB}/rl_renderer_core.cpp
    ${RENDER_LIB}/rl_renderer_metrics.cpp
    ${RENDER_LIB}/rl_scene.cpp
    ${RENDER_LIB}/rl_scene_entity.cpp
    ${RENDER_LIB}/rl_scene_renderer.cpp
    ${RENDER_LIB}/rl_scene_renderer_lighting.cpp
    ${RENDER_LIB}/rl_scene_shadows.cpp
    ${RENDER_LIB}/rl_compositor.cpp
    ${RENDER_LIB}/rl_hdr_compositor.cpp
    ${RENDER_LIB}/rl_outline_compositor.cpp
    ${RENDER_LIB}/rl_depth_of_field.cpp
    ${RENDER_LIB}/rl_distortion.cpp
    ${RENDER_LIB}/rl_motion_blur.cpp
    ${RENDER_LIB}/rl_ssao.cpp
    ${RENDER_LIB}/rl_decal.cpp
    ${RENDER_LIB}/rl_particle_renderer_deferred.cpp
    # VFX.md FX0: stubs (file statics only) of the particle, corona, planar decal and layered material TUs, ported in
    # FX2 (particles, coronas), FX4 (layered materials) and FX7 (planar decals)
    ${RENDER_LIB}/rl_corona_instance.cpp
    ${RENDER_LIB}/rl_layered_material.cpp
    ${RENDER_LIB}/rl_particle.cpp
    ${RENDER_LIB}/rl_particle_batch.cpp
    ${RENDER_LIB}/rl_particle_filler_billboard.cpp
    ${RENDER_LIB}/rl_particle_filler_drop.cpp
    ${RENDER_LIB}/rl_particle_filler_oriented.cpp
    ${RENDER_LIB}/rl_particle_filler_radial.cpp
    ${RENDER_LIB}/rl_particle_filler_ribbon.cpp
    ${RENDER_LIB}/rl_planar_decal.cpp
    ${RENDER_LIB}/rl_renderable_particles.cpp
    ${RENDER_LIB}/rl_renderable_particles_instance.cpp
    ${RENDER_LIB}/rl_renderable_particles_instance_group.cpp
    ${RFG}/camera/rfg_camera_render.cpp
    # V5: game_render.cpp, the time of day and its shared data in one unity TU (shared file statics)
    ${UNITY}/rfg_gamewide_render_unity.cpp
    # V5: zone lights (obj_light creates rl_lights; out of the GPU-free objects unity)
    ${RFG}/objects/object_light.cpp
    ${RFG}/objects/object_skybox.cpp
    ${RFG}/objects/object_skybox_shared.cpp
    # P3: the render half of the character meshes
    ${RFG}/geom/character_mesh_shared_render.cpp
    # lod.md L2: the foliage meshes (the undergrowth's clone meshes)
    ${RFG}/geom/foliage_mesh.cpp
    ${RFG}/geom/foliage_mesh_shared.cpp
    ${RENDER_LIB}/rl_shader.cpp
    ${RENDER_LIB}/rl_shader_library.cpp
    ${RENDER_LIB}/rl_shadow_system.cpp
    ${RENDER_LIB}/rl_skin_instance.cpp
    ${RENDER_LIB}/rl_skybox_renderable_instance.cpp
    ${RENDER_LIB}/rl_star_field.cpp
    ${RENDER_LIB}/rl_state_manager.cpp
    ${RENDER_LIB}/rl_terrain_renderer.cpp
    # V4: the terrain renderables and the render half of game_render_terrain.cpp
    ${RENDER_LIB}/rl_height_mesh_instance.cpp
    ${RENDER_LIB}/rl_landmark_lod_mesh_instance.cpp
    ${RENDER_LIB}/rl_low_lod_mesh_instance.cpp
    ${RENDER_LIB}/rl_skirt_mesh_instance.cpp
    ${RENDER_LIB}/rl_terrain_occluder.cpp
    ${RFG}/game_render_terrain_render.cpp
    ${RENDER_LIB}/rl_texture.cpp
    ${RENDER_LIB}/rl_vertex_formats.cpp
    ${RENDER_LIB}/rl_visibility_manager.cpp
    ${RENDER_LIB}/rl_xray_material_manager.cpp
    ${SHADER_VARIANTS}/clbprojector_sv.cpp
    ${SHADER_VARIANTS}/clbstencilopt_sv.cpp
    # the terrain render context's shader variants (V2 needs its kg shaders for the streamed meshes' materials)
    ${SHADER_VARIANTS}/eosglass_sv.cpp
    ${SHADER_VARIANTS}/glass_sv.cpp
    ${SHADER_VARIANTS}/heightmesh_sv.cpp
    ${SHADER_VARIANTS}/heightmeshlowlod_sv.cpp
    ${SHADER_VARIANTS}/sidemap_sv.cpp
    ${SHADER_VARIANTS}/substance_sv.cpp
    ${SHADER_VARIANTS}/terrainblend_sv.cpp
    ${SHADER_VARIANTS}/terrainroads_sv.cpp
    ${SHADER_VARIANTS}/terrainrocks_sv.cpp
    ${OPENRFG_SRC}/generated/rfgdata_tables.cpp
)
target_link_libraries(rfg_render PUBLIC rfg_multi_alone rfg keen_graphics)

# ---- the effect system (VFX.md 2.6): vfxlib, object_effect, the code effects (bullet sparks, tracers, dust, glass,
# material effects and the standalone materials their layers use), object decals, mover faders and the radial blur.
# Only OpenRFG, viewer and viewer_mp link it, before rfg_render; every other program keeps the effect stand-ins
# (deterministic hashes, VFX.md 2.5), and a duplicate definition fails their link. FX1-FX3: vfxlib, object_effect and the effect playback, weather and wind;
# the later tiers' files are still stubs (file statics; the functions are under #if 0).
set(VFXLIB ${OPENRFG_SRC}/volition/vfxlib/code)
add_library(rfg_effects STATIC
    ${VFXLIB}/vfx.cpp
    ${VFXLIB}/vfx_effect.cpp
    ${VFXLIB}/vfx_effect_expressions.cpp
    ${VFXLIB}/vfx_effect_manager.cpp
    ${VFXLIB}/vfx_effect_template.cpp
    ${VFXLIB}/vfx_particle_system.cpp
    ${VFXLIB}/vfx_particle_system_emitter.cpp
    ${VFXLIB}/vfx_particle_system_emitter_box.cpp
    ${VFXLIB}/vfx_particle_system_emitter_cylinder.cpp
    ${VFXLIB}/vfx_particle_system_emitter_instance.cpp
    ${VFXLIB}/vfx_particle_system_emitter_plane.cpp
    ${VFXLIB}/vfx_particle_system_emitter_point.cpp
    ${VFXLIB}/vfx_particle_system_emitter_sphere.cpp
    ${VFXLIB}/vfx_particle_system_filter.cpp
    ${VFXLIB}/vfx_particle_system_filter_instance.cpp
    ${VFXLIB}/vfx_particle_system_filter_magnet.cpp
    ${VFXLIB}/vfx_particle_system_filter_orientor.cpp
    ${VFXLIB}/vfx_particle_system_filter_turbulence.cpp
    ${VFXLIB}/vfx_particle_system_filter_vortex.cpp
    ${VFXLIB}/vfx_particle_system_filter_wind.cpp
    ${VFXLIB}/vfx_particle_system_manager.cpp
    ${VFXLIB}/vfx_renderable_filter_instance.cpp
    ${RFG}/effects/effects.cpp
    ${RFG}/effects/object_decal.cpp
    ${RFG}/effects/object_effect.cpp
    ${RFG}/effects/object_effect_profile.cpp
    ${RFG}/code_effects/bullet_sparks.cpp
    # stub_impl.md S3: distortion, glare (table and lists), line of sight blockers, x-ray
    ${RFG}/code_effects/distortion_effect.cpp
    ${RFG}/code_effects/glare.cpp
    ${RFG}/code_effects/los_blocker.cpp
    ${RFG}/code_effects/xray_effect.cpp
    ${RFG}/code_effects/dust.cpp
    ${RFG}/code_effects/glass.cpp
    ${RFG}/code_effects/material_effects.cpp
    ${RFG}/code_effects/material_effects_shared.cpp
    ${RFG}/code_effects/material_preload_shared.cpp
    ${RFG}/code_effects/tracers.cpp
    ${RFG}/game_render/fullscreen_effects/fb_accum_blur.cpp
    ${RFG}/game_render/fullscreen_effects/fb_color_correct.cpp
    # VFX.md FX7: decal_info.cpp is a split TU (rfg has the table part)
    ${RFG}/geom/decal_info.cpp
    ${RFG}/geom/material_shared.cpp
    ${RFG}/geom/rfg_material.cpp
    ${RFG}/objects/movers/mover_fader.cpp
    ${RFG}/weather/weather.cpp
    ${RFG}/weather/wind.cpp
)
target_link_libraries(rfg_effects PUBLIC rfg_render rfg)
# FX3: effects.cpp, weather.cpp and wind.cpp are split TUs, compiled by rfg for their table/state part and here, with
# OPENRFG_EFFECTS, for the rest. The definition is public: the programs linking the library compile their shared
# stand-in files with it, which drops the stand-ins the library replaces.
target_compile_definitions(rfg_effects PUBLIC OPENRFG_EFFECTS)

# ---- the audio system (SFX.md 2.6): Volition's audio_lib (audio_lib_unity in rfg.exe) with its keen hardware layer,
# gameaudio, gamemusic and the human voice. Only OpenRFG, viewer and viewer_mp link it; every other program keeps the
# audio stand-ins (simulation-thread rand() and wall-clock play states, SFX.md 2.5), and a duplicate definition fails
# their link. OPENRFG_AUDIO gates the shared stand-in files as OPENRFG_EFFECTS does. SX2: audio_lib is ported whole
# as one unity TU (shared file statics), as in rfg.exe. SX3: gameaudio is ported whole, game_options.cpp is a split TU
# (the rfg library has the cache and the ini reader, this one the audio part of game_options_load_from_cache);
# human_voice was a stub until SX6. SX4: vanim.cpp is a split TU too (the rfg library has the animation state
# machine, this one the anim sound, voice and prop triggers). SX5: gamemusic.cpp (front end and multiplayer music).
# SX6: the human_voice files whole and the dialogue manager (the announcer plays through it).
add_library(rfg_audio STATIC
    ${UNITY}/audio_lib_unity.cpp
    ${RFG}/gamewide/gameaudio.cpp
    ${RFG}/gamewide/game_options.cpp
    ${RFG}/gamewide/gamemusic.cpp
    ${RFG}/geom/vanim.cpp
    ${RFG}/objects/humans/human_voice/human_voice.cpp
    ${RFG}/objects/humans/human_voice/human_voice_lines.cpp
    ${RFG}/objects/humans/human_voice/human_voice_shared.cpp
    ${RFG}/objects/humans/human_voice/voice_line_unlocking.cpp
    ${RFG}/scripting/dialogue_manager.cpp
)
target_link_libraries(rfg_audio PUBLIC keen_sound rfg)
target_compile_definitions(rfg_audio PUBLIC OPENRFG_AUDIO)
# The Pegs callbacks are the only part of rfg.cpp the render programs need (they do not run the original game loop).
set_source_files_properties(${RFG}/rfg.cpp PROPERTIES COMPILE_DEFINITIONS OPENRFG_RFG_PEG_CALLBACKS_ONLY)

# ---- the original game loop and boot (multiplayer.md MP1, orig_main.md OM3 and OM4): rfg.cpp's frame, both boot stages,
# the shutdown and the globals it defines, the game state machine, the gameplay, loading, boot screen and multiplayer
# switch states, the handshake, the game clock, the frame time, the render thread (in the gamewide unity) and the vlib
# scheduler with its threads (part of vlib_unity in rfg.exe). Only OpenRFG links it; the headless tests keep their
# hand-ordered frame (tests/headless/headless_game.cpp) and the inline scheduler of tests/stubs/vlib/vlib_noboot.cpp.
add_library(rfg_loop STATIC
    ${UNITY}/rfg_gamewide_unity.cpp
    ${UNITY}/rfg_unity.cpp
    ${RFG}/interface/loading.cpp
    ${VLIB}/os/frametime.cpp
    ${VLIB}/os/scheduler.cpp
)
# keen_gameframework: rfg_init_keen_memory_allocators (OM1) reads the framework's memory system and default allocator.
target_link_libraries(rfg_loop PUBLIC rfg keen_gameframework)

# ---- keen::rfg::Application whole (orig_main.md OM3) and the boot's game options, display options and dlmalloc
# init. application.cpp is compiled again with only the part the rfg library leaves out (OPENRFG_APPLICATION_MAIN).
add_library(rfg_app STATIC
    ${OPENRFG_SRC}/game/sources/application.cpp
    ${RFG}/gamewide/game_options.cpp
    ${RFG}/interface/option_menus/options_display.cpp
)
target_compile_definitions(rfg_app PRIVATE OPENRFG_APPLICATION_MAIN)
# vlua: dl_system_init (os/dlmalloc moved there whole in UI1)
target_link_libraries(rfg_app PUBLIC rfg_loop keen_gameframework vlua)

# ---- UI.md UI0: the original UI's libraries, so far only the compiled stubs (file statics; the functions stay under
# #if 0 until their tier ports them). vlua is whole since UI1 (rfg_app links it for dl_system_init), vint comes in with
# UI2's test, rfg_ui with UI3 (OpenRFG). vlua: Volition's Lua 5.0, lua_lib and os/dlmalloc;
# vint: interface_lib; rfg_ui: the game's interface, HUD, multiplayer and option menus.
# OPENRFG_UI gates the shared stand-in files as OPENRFG_EFFECTS and OPENRFG_AUDIO do.
set(LUA ${OPENRFG_SRC}/volition/libmodules/lua/lua_1_21)
add_library(vlua STATIC
    ${LUA}/lapi.cpp ${LUA}/lauxlib.cpp ${LUA}/lbaselib.cpp ${LUA}/lcode.cpp ${LUA}/ldblib.cpp ${LUA}/ldebug.cpp
    ${LUA}/ldo.cpp ${LUA}/ldump.cpp ${LUA}/lfunc.cpp ${LUA}/lgc.cpp ${LUA}/llex.cpp ${LUA}/lmathlib.cpp ${LUA}/lmem.cpp
    ${LUA}/lobject.cpp ${LUA}/lparser.cpp ${LUA}/lstate.cpp ${LUA}/lstring.cpp ${LUA}/lstrlib.cpp ${LUA}/ltable.cpp
    ${LUA}/ltablib.cpp ${LUA}/ltm.cpp ${LUA}/lundump.cpp ${LUA}/lvm.cpp ${LUA}/lzio.cpp ${LUA}/lopcodes.cpp
    # UI1: lua_lib as rfg.exe compiles it (one unity TU, shared file statics)
    ${UNITY}/lua_lib_unity.cpp
    # UI1: Volition's dlmalloc (Lua's heap; my_sbrk reads Level_memmgr, which the program provides)
    ${RFG}/os/dlmalloc/dmalloc.cpp
)
target_link_libraries(vlua PUBLIC vlib)

set(VINT ${OPENRFG_SRC}/volition/interface_lib/code)
# UI2: interface_lib as rfg.exe compiles it (one unity TU, shared file statics)
add_library(vint STATIC ${UNITY}/interface_lib_unity.cpp)
target_link_libraries(vint PUBLIC vlua)

add_library(rfg_ui STATIC
    ${RFG}/gamewide/game_hudtimer.cpp
    # The save system on keen's save data, the import of the original release's saves and the verify state
    ${RFG}/gamewide/game_save_generic.cpp
    ${RFG}/gamewide/game_save_importer.cpp
    ${RFG}/gamewide/rfgsaveinterface.cpp
    ${RFG}/gamewide/verify_gamesave.cpp
    # stub_impl.md S5: the quick pause state (a message box)
    ${RFG}/gamewide/gamestate_quick_pause.cpp
    ${RFG}/hud/fog_of_war.cpp
    ${RFG}/hud/fullscreen_map.cpp
    ${RFG}/hud/in_world_gps.cpp
    ${RFG}/hud/in_world_gps_shared.cpp
    ${RFG}/hud/hud_ui_activity_briefing.cpp
    ${RFG}/hud/hud_ui_activity_complete.cpp
    ${RFG}/hud/hud_ui_district_liberation.cpp
    ${RFG}/hud/hud_ui_handbook.cpp
    ${RFG}/hud/hud_ui_mission_briefing.cpp
    ${RFG}/hud/hud_ui_mission_complete.cpp
    ${RFG}/hud/hud_ui_upgrades.cpp
    ${RFG}/hud/hud_ui_weapon_cabinet.cpp
    ${RFG}/hud/hud.cpp
    ${RFG}/hud/hud_floating_text.cpp
    ${RFG}/hud/hud_heartbeat_effect.cpp
    ${RFG}/hud/hud_icon.cpp
    ${RFG}/hud/hud_message.cpp
    ${RFG}/hud/minimap.cpp
    ${RFG}/hud/minimap_icon_info.cpp
    ${RFG}/hud/objectives.cpp
    # UI4: the player profile (the other programs keep their stand-ins)
    ${RFG}/gamewide/player_profile.cpp
    ${RFG}/interface/credits.cpp
    # orig_main.md OM7: the in-game interface (the pause push, the letterbox), the overlay stack and the weapon selection
    # overlay (the weapon menu, 1-4 switching, the quick switch)
    ${RFG}/interface/ingame/interface.cpp
    ${RFG}/interface/ingame/overlay.cpp
    ${RFG}/interface/ingame/ovl_inventory.cpp
    ${RFG}/interface/localization/localization.cpp
    ${RFG}/interface/localization/localization_shared.cpp
    ${RFG}/interface/main_menu.cpp
    ${RFG}/interface/main_menu_idle.cpp
    ${RFG}/interface/ui_startup_video.cpp
    # stub_impl.md S7: the Bink video player, on the shipped binkw32.dll
    ${RFG}/video_player/video_player.cpp
    # stub_impl.md S9: the new game's opening movie and the scripts' movies
    ${RFG}/cutscene/video_cutscene.cpp
    ${RFG}/interface/menu_transitions.cpp
    ${RFG}/interface/multi_player_menus/multi_player_menu_system.cpp
    ${RFG}/interface/multi_player_menus/ui_document_mp_character_select.cpp
    ${RFG}/interface/multi_player_menus/ui_document_mp_game_lobby.cpp
    ${RFG}/interface/multi_player_menus/ui_document_mp_game_options.cpp
    ${RFG}/interface/multi_player_menus/ui_document_mp_live.cpp
    ${RFG}/interface/multi_player_menus/ui_document_mp_live_find_servers.cpp
    ${RFG}/interface/multi_player_menus/ui_document_mp_change_matchmaking.cpp
    ${RFG}/interface/multi_player_menus/ui_document_mp_matchmaking.cpp
    ${RFG}/interface/multi_player_menus/ui_document_mp_party_lobby.cpp
    ${RFG}/interface/multi_player_menus/ui_document_mp_party_options.cpp
    ${RFG}/interface/multi_player_menus/ui_document_mp_results.cpp
    ${RFG}/interface/multi_player_menus/ui_document_mp_scoreboard.cpp
    ${RFG}/interface/multi_player_menus/ui_document_mp_tutorials.cpp
    ${RFG}/interface/multi_player_menus/ui_document_mp_stats.cpp
    ${RFG}/interface/multi_player_menus/ui_document_mp_syslink.cpp
    ${RFG}/interface/multi_player_menus/ui_document_mp_syslink_find_servers.cpp
    ${RFG}/interface/multi_player_menus/ui_document_mp_xp_tally.cpp
    ${RFG}/interface/option_menus/cheats_menu.cpp
    ${RFG}/interface/option_menus/controller_options.cpp
    ${RFG}/interface/option_menus/extras_menu.cpp
    ${RFG}/interface/option_menus/sony_options.cpp
    # UI6: the display menus (the boot part is rfg_app's)
    ${RFG}/interface/option_menus/options_display.cpp
    ${RFG}/interface/option_menus/ingame_options.cpp
    ${RFG}/interface/option_menus/options_audio.cpp
    ${RFG}/interface/option_menus/options_game_options.cpp
    ${RFG}/interface/option_menus/options_main.cpp
    ${RFG}/interface/option_menus/options_xlive.cpp
    ${RFG}/interface/option_menus/options_xlive_quickmatch.cpp
    ${RFG}/interface/option_menus/options_pc.cpp
    ${RFG}/interface/option_menus/player_death_options.cpp
    ${RFG}/interface/ui_character.cpp
    ${RFG}/interface/ui_document_hud.cpp
    ${RFG}/interface/ui_document_hud_multiplayer.cpp
    ${RFG}/interface/ui_document_hud_satellite.cpp
    ${RFG}/interface/ui_document_hud_missions.cpp
    ${RFG}/interface/ui_document_hud_rail_driver.cpp
    ${RFG}/interface/ui_document_hud_sniper.cpp
    ${RFG}/interface/ui_document_hud_tool_tips.cpp
    ${RFG}/interface/ui_document_hud_weapons.cpp
    # wrecking.md WC3: the Wrecking Crew HUD
    ${RFG}/interface/ui_document_hud_wrecking_crew.cpp
    ${RFG}/interface/ui_document_manager.cpp
    # wrecking.md WC2: the Wrecking Crew menus (main menu, character select, scoreboard)
    ${RFG}/interface/ui_document_menu_wrecking_crew.cpp
    ${RFG}/interface/ui_document_menu_wrecking_crew_character_select.cpp
    ${RFG}/interface/ui_document_menu_wrecking_crew_scoreboard.cpp
    ${RFG}/interface/ui_document_save_load.cpp
    ${RFG}/interface/ui_document_shared.cpp
    ${RFG}/interface/ui_document_stats.cpp
    ${RFG}/interface/ui_document_view_dlc.cpp
    ${RFG}/interface/ui_memory_manager.cpp
    ${RFG}/interface/ui_menu.cpp
    ${RFG}/interface/ui_message_box.cpp
    ${RFG}/interface/vint/rfg_lua_actions.cpp
    ${RFG}/interface/vint/vint_rfg.cpp
)
target_compile_definitions(rfg_ui PUBLIC OPENRFG_UI)
target_link_libraries(rfg_ui PUBLIC vint rfg_render rfg platform_binkw32 platform_sw_api)

# ---- the multiplayer transport (multiplayer.md MP2): keen network (winsock sockets, addresses, the platform start-up),
# the GameLink base and its LAN implementation, the remote game session and the parts of the local game session, the
# system services, the interaction loca tags and the save data handler it calls. Only the multiplayer programs link it.
set(RUNTIME ${OPENRFG_RUNTIME})
add_library(rfg_net STATIC
    ${RUNTIME}/network/sources/global/networkaddress.cpp
    ${RUNTIME}/network/sources/posix/posixsocket.cpp
    ${RUNTIME}/network/sources/win32/networkplatform_win32.cpp
    ${RUNTIME}/gamelink/sources/global/gamelink.cpp
    ${RUNTIME}/gamelink/sources/global/lan/gamelinklan.cpp
    ${RUNTIME}/gamesession/sources/global/localgamesession.cpp
    ${RUNTIME}/gamesession/sources/global/remotegamesession.cpp
    ${RUNTIME}/systemservices/sources/global/systemservices.cpp
    ${RUNTIME}/systemservices/sources/steam/systemservices_steam.cpp
    ${RUNTIME}/gamesessioninteractions/sources/global/gamesessioninteractions.cpp
    ${RUNTIME}/savedata/sources/global/savedatahandler.cpp
    # stub_impl.md S2: the keen platform services on Steam (sw_api.dll): the Steamworks system, the user accounts, the
    # save data (Steam remote storage), the system services (achievements, stats, rankings, presence) and the internet
    # link (Steam lobbies, P2P, voice, VAC auth)
    ${RUNTIME}/steam/sources/global/steamworkssystem.cpp
    ${RUNTIME}/useraccount/sources/global/useraccountsystem.cpp
    ${RUNTIME}/useraccount/sources/steam/useraccounthelpers_steam.cpp
    ${RUNTIME}/useraccount/sources/steam/useraccountsystem_steam.cpp
    ${RUNTIME}/savedata/sources/global/savedatasystem.cpp
    ${RUNTIME}/savedata/sources/win32/savedatasystem_win32.cpp
    # application.cpp deviation 4: the local save data
    ${RUNTIME}/savedata/sources/win32/savedataprovider_win32_native.cpp
    ${RUNTIME}/savedata/sources/win32/steam/savedataprovider_win32_steam.cpp
    ${RUNTIME}/systemservices/sources/steam/onlineachievements_steam.cpp
    ${RUNTIME}/systemservices/sources/steam/onlinehelpers_steam.cpp
    ${RUNTIME}/systemservices/sources/steam/onlinestats_steam.cpp
    ${RUNTIME}/gamelink/sources/steam/gamelinkinternet_steam.cpp
)
# keen_sound: XAudio2 2.7 for the voice chat; ole32/uuid: the network list manager (systemservices_steam.cpp).
target_link_libraries(rfg_net PUBLIC keen_base keen_sound platform_sw_api ws2_32 ole32 uuid)

# ---- the vehicle/ tree as rfg.exe compiles it (stub_impl.md S8): one unity TU, only OpenRFG links it. It needs the
# audio and effects tables, which the test programs do not link; they keep stand-ins in tests/stubs.
add_library(rfg_vehicle STATIC ${UNITY}/rfg_vehicle_unity.cpp)
target_link_libraries(rfg_vehicle PUBLIC rfg rfg_audio)

# stub_impl.md S9 to S11: the single player game. Linked by OpenRFG only: the test programs keep their stand-ins, so
# their baselines do not move.
add_library(rfg_gameplay STATIC
    ${RFG}/camera/camera_cutscene.cpp
    ${RFG}/camera/camera_satellite.cpp
    ${RFG}/cutscene/cutscene.cpp
    ${RFG}/cutscene/cutscene_audio.cpp
    ${RFG}/cutscene/cutscene_load.cpp
    ${RFG}/cutscene/cutscene_options.cpp
    ${RFG}/cutscene/cutscene_timer.cpp
    ${RFG}/gameplay/activity_mission_menu.cpp
    ${RFG}/gameplay/difficulty.cpp
    ${RFG}/geom/simple_keyframe.cpp
    # Partial: its save section
    ${RFG}/scripting/public_address.cpp
    ${RFG}/gameplay/gameplay.cpp
    ${RFG}/gameplay/mission_info.cpp
    ${RFG}/gameplay/missions.cpp
    ${RFG}/gameplay/restore_list.cpp
    ${RFG}/gameplay/road_distance_info.cpp
    ${RFG}/gameplay/safehouse_vip.cpp
    # gameflow.md GF3
    ${RFG}/gameplay/air_bombing.cpp
    ${RFG}/gameplay/marauder_raid.cpp
    ${RFG}/gameplay/vehicle_drop_off.cpp
    ${RFG}/gameplay/gameflow/alert_level_manager.cpp
    ${RFG}/gameplay/gameflow/continuous_support.cpp
    ${RFG}/gameplay/gameflow/gameflow_manager.cpp
    ${RFG}/gameplay/gameflow/gfm_control_morale.cpp
    ${RFG}/gameplay/gameflow/gfm_damage_report.cpp
    ${RFG}/gameplay/gameflow/gfm_structure_defense_coordinator.cpp
    ${RFG}/gameplay/gameflow/gfm_tech_level.cpp
    ${RFG}/gameplay/gameflow/reinforcement_location.cpp
    ${RFG}/gameplay/gameflow/squad_loadout.cpp
    # gameflow.md GF4
    ${RFG}/gameplay/activities/activity.cpp
    ${RFG}/gameplay/activities/activity_area_defense.cpp
    ${RFG}/gameplay/activities/activity_assassin.cpp
    ${RFG}/gameplay/activities/activity_car_bomb.cpp
    ${RFG}/gameplay/activities/activity_continuous_attack.cpp
    ${RFG}/gameplay/activities/activity_convoy.cpp
    ${RFG}/gameplay/activities/activity_courier_chase.cpp
    ${RFG}/gameplay/activities/activity_delivery.cpp
    ${RFG}/gameplay/activities/delivery_common.cpp
    ${RFG}/gameplay/activities/activity_demolitions_master.cpp
    ${RFG}/gameplay/activities/activity_house_arrest.cpp
    ${RFG}/gameplay/activities/activity_killing_spree.cpp
    ${RFG}/gameplay/activities/activity_mayhem.cpp
    ${RFG}/gameplay/activities/activity_mission_vrail.cpp
    ${RFG}/gameplay/activities/activity_raid.cpp
    ${RFG}/gameplay/activities/activity_raid_attack.cpp
    ${RFG}/gameplay/activities/activity_raid_defend.cpp
    ${RFG}/gameplay/activities/activity_register_damage.cpp
    ${RFG}/gameplay/activities/activity_spawn.cpp
    ${RFG}/gameplay/activities/activity_tanks_a_lot.cpp
    ${RFG}/gameplay/activities/activity_vrail.cpp
    ${RFG}/gameplay/activities/vrail_common.cpp
    ${RFG}/gameplay/activities/vrail_data.cpp
    ${RFG}/gameplay/activities/vrail_system.cpp
    ${RFG}/gameplay/activities/base_activity_data.cpp
    ${RFG}/gameplay/activities/gfm_reinforcement_coordinator.cpp
    ${RFG}/gameplay/activities/gfm_smash_building_coordinator.cpp
    ${RFG}/gameplay/activities/morale_effect_manager.cpp
    ${RFG}/gameplay/activities/raid_common.cpp
    ${RFG}/gameplay/activities/reinforcement_squad.cpp
    ${RFG}/objects/critical_area.cpp
    ${RFG}/objects/object_activity_spawn.cpp
    ${RFG}/objects/object_air_strike_defense_node.cpp
    ${RFG}/objects/object_area_defense_node.cpp
    ${RFG}/objects/object_bftp_node.cpp
    ${RFG}/objects/object_mission_start_node.cpp
    ${RFG}/objects/object_courier_end_point.cpp
    ${RFG}/objects/object_delivery_node.cpp
    ${RFG}/objects/object_demolitions_master_node.cpp
    ${RFG}/objects/object_house_arrest_node.cpp
    ${RFG}/objects/object_riding_shotgun_node.cpp
    ${RFG}/objects/object_raid_node.cpp
    ${RFG}/objects/object_roadblock_node.cpp
    ${RFG}/objects/object_safehouse.cpp
    ${RFG}/objects/object_spawn_region.cpp
    ${RFG}/objects/object_upgrade_node.cpp
    ${RFG}/havok/havok_filter_shape_collection.cpp
    ${RFG}/objects/humans/npc.cpp
    ${RFG}/objects/object_navpoint.cpp
    ${RFG}/objects/object_patrol.cpp
    ${RFG}/objects/object_spawn_node.cpp
    ${RFG}/gameplay/activities/activity_rewards.cpp
    ${RFG}/objects/object_convoy.cpp
    ${RFG}/objects/object_convoy_end_point.cpp
    ${RFG}/objects/object_squad.cpp
    ${RFG}/objects/object_trigger_region.cpp
    ${RFG}/gameplay/gameflow/gfm_orders_manager.cpp
    ${RFG}/gameplay/gameflow/squad_progression.cpp
    ${RFG}/gameplay/gameflow/squad_spawn_manager.cpp
    ${RFG}/gameplay/squad_spawn.cpp
    ${RFG}/scripting/managed_script_group.cpp
    ${RFG}/scripting/objective_highlight.cpp
    ${RFG}/scripting/target_outline.cpp
    ${RFG}/scripting/action_script/action_script.cpp
    ${RFG}/scripting/action_script/global_event_manager.cpp
    ${RFG}/scripting/action_script/script_action.cpp
    ${RFG}/scripting/action_script/script_definitions.cpp
    ${RFG}/scripting/action_script/script_event.cpp
    ${RFG}/scripting/action_script/script_function.cpp
    ${RFG}/scripting/action_script/script_timer.cpp
    ${RFG}/scripting/action_script/script_types.cpp
    ${RFG}/spawn/ambient_spawn_info.cpp
    ${RFG}/spawn/spawn_object_group.cpp
    ${RFG}/spawn/spawn_placement.cpp
    ${RFG}/spawn/spawn_request.cpp
    ${RFG}/spawn/spawn_resource_data.cpp
    ${RFG}/spawn/spawn_resource_manager.cpp
    # wrecking.md WC1: the Wrecking Crew core and its five modes (the mode table links them all)
    ${RFG}/gameplay/wrecking_crew/wrecking_crew.cpp
    ${RFG}/gameplay/wrecking_crew/wrecking_crew_internal.cpp
    ${RFG}/gameplay/wrecking_crew/wrecking_crew_mode_fall_guy.cpp
    ${RFG}/gameplay/wrecking_crew/wrecking_crew_mode_mayhem.cpp
    ${RFG}/gameplay/wrecking_crew/wrecking_crew_mode_rampage.cpp
    ${RFG}/gameplay/wrecking_crew/wrecking_crew_mode_total_chaos.cpp
    ${RFG}/gameplay/wrecking_crew/wrecking_crew_mode_walker.cpp
)
target_link_libraries(rfg_gameplay PUBLIC rfg)

# ---- stub_impl.md S9: AI, navigation, spawning and traffic of the single player game, linked by OpenRFG only like
# rfg_gameplay, so the test programs keep their stand-ins and baselines.
add_library(rfg_ai STATIC
    ${RFG}/objects/humans/human_move_pathfind.cpp
    ${RFG}/objects/humans/npc_sound_info.cpp
    ${RFG}/spawn/ambient_prop_info.cpp
    ${RFG}/spawn/ambient_spawn_behavior.cpp
    ${RFG}/spawn/ambient_spawn_manager.cpp
    ${RFG}/objects/object_action_node.cpp
    ${RFG}/objects/humans/human_combat_cover.cpp
    ${RFG}/objects/object_cover_node.cpp
    ${RFG}/objects/object_dummy.cpp
    ${RFG}/objects/object_marauder_ambush_region.cpp
    ${RFG}/objects/object_restricted_area.cpp
    ${RFG}/objects/humans/player_cover.cpp
    ${RFG}/objects/object_guard_node.cpp
    ${RFG}/objects/object_path_road.cpp
    ${RFG}/ai/ai_actions.cpp
    ${RFG}/ai/ai_scripting.cpp
    ${RFG}/ai/ai_building_logic.cpp
    ${RFG}/ai/ai_script_actions.cpp
    ${RFG}/ai/combat/combat_actions.cpp
    ${RFG}/ai/combat/combat_draw_reload_actions.cpp
    ${RFG}/ai/ai_planning_data.cpp
    ${RFG}/ai/combat/combat_common.cpp
    ${RFG}/ai/combat/combat_cover.cpp
    ${RFG}/ai/combat/combat_cover_actions.cpp
    ${RFG}/ai/combat/cover_selection.cpp
    ${RFG}/ai/combat/cover_selection_internal.cpp
    ${RFG}/ai/combat/combat_maneuver.cpp
    ${RFG}/ai/combat/combat_maneuver_actions.cpp
    ${RFG}/ai/combat/combat_melee_actions.cpp
    ${RFG}/ai/combat/ai_thrown_weapon_logic.cpp
    ${RFG}/ai/combat/combat_thrown_actions.cpp
    ${RFG}/ai/combat/combat_turret_actions.cpp
    ${RFG}/ai/combat/defensive_combat_actions.cpp
    ${RFG}/ai/combat/dodge_actions.cpp
    ${RFG}/ai/combat/flush_out_actions.cpp
    ${RFG}/ai/combat/acquire_visual_actions.cpp
    ${RFG}/ai/reaction_actions.cpp
    ${RFG}/ai/search_actions.cpp
    ${RFG}/ai/search_system.cpp
    ${RFG}/ai/search_system_internal.cpp
    ${RFG}/ai/search_utils.cpp
    ${RFG}/ai/investigation_actions.cpp
    ${RFG}/ai/self_preservation_actions.cpp
    ${RFG}/ai/threaten_actions.cpp
    ${RFG}/ai/custom_actions.cpp
    ${RFG}/ai/civilian/civilian_restricted_area_actions.cpp
    ${RFG}/ai/civilian/guerrilla_actions.cpp
    ${RFG}/ai/civilian/pedestrian_actions.cpp
    ${RFG}/ai/civilian/civilian_common.cpp
    ${RFG}/ai/civilian/ambient_common.cpp
    ${RFG}/ai/ambient_edf_action_manager.cpp
    ${RFG}/ai/civilian/ambient_actions.cpp
    ${RFG}/ai/civilian/ambient_edf_action.cpp
    ${RFG}/ai/road_route.cpp
    ${RFG}/ai/ai_defense_coordination.cpp
    ${RFG}/ai/ai_defense_coordination_internal.cpp
    ${RFG}/ai/formation.cpp
    ${RFG}/ai/formation_internal.cpp
    ${RFG}/ai/goals.cpp
    ${RFG}/ai/despawn_actions.cpp
    ${RFG}/ai/escape_actions.cpp
    ${RFG}/ai/follow_actions.cpp
    ${RFG}/ai/follow_logic.cpp
    ${RFG}/ai/guard_actions.cpp
    ${RFG}/ai/guard_selection.cpp
    ${RFG}/ai/misc_actions.cpp
    ${RFG}/ai/vehicle/ai_vehicle_logic.cpp
    ${RFG}/ai/vehicle/vehicle_ai_actions.cpp
    ${RFG}/vehicle_ai/air_traffic_control.cpp
    ${RFG}/vehicle_ai/intersection_controller.cpp
    ${RFG}/vehicle_ai/lane_heap.cpp
    ${RFG}/vehicle_ai/lane_pathfind.cpp
    ${RFG}/vehicle_ai/vehicle_ai.cpp
    ${RFG}/vehicle_ai/vehicle_idle_ai.cpp
    ${RFG}/vehicle_ai/vehicle_lane_common.cpp
    ${RFG}/vehicle_ai/vehicle_navmesh_common.cpp
    ${RFG}/vehicle_ai/vehicle_navmesh_path_create.cpp
    ${RFG}/vehicle_ai/vehicle_offscreen_pathfollow.cpp
    ${RFG}/vehicle_ai/vehicle_path_request.cpp
    ${RFG}/vehicle_ai/vehicle_repulsor.cpp
    ${RFG}/traffic/traffic.cpp
    ${OPENRFG_SRC}/volition/vlib/code/util/recursive_bbox.cpp
    ${RFG}/ai/reinforcement_actions.cpp
    ${RFG}/ai/patrol_actions.cpp
    ${RFG}/ai/self_preservation_goals.cpp
    ${RFG}/ai/hazard_detection.cpp
    ${RFG}/ai/planning.cpp
    ${RFG}/ai/ai_rate_limiters.cpp
    ${RFG}/ai/alerts.cpp
    ${RFG}/ai/ambient_action_manager.cpp
    ${RFG}/ai/ai_system.cpp
    ${RFG}/ai/los_queue.cpp
    ${RFG}/ai/percept.cpp
    ${RFG}/ai/beliefs.cpp
    ${RFG}/ai/plan_conditions.cpp
    ${RFG}/ai/player_attack_limiter.cpp
    ${RFG}/pathfinding/nav_cell.cpp
    ${RFG}/pathfinding/bb_pathfinding.cpp
    ${RFG}/pathfinding/nav_cell_detour.cpp
    ${RFG}/pathfinding/nav_heap.cpp
    ${RFG}/pathfinding/road_heap.cpp
)
target_link_libraries(rfg_ai PUBLIC rfg)

# ---- the multi/ tree as rfg.exe compiles it: one unity TU (rfg_multi_unity.cpp, the files ported so far). Only the
# multiplayer programs link it.
add_library(rfg_multi STATIC
    ${UNITY}/rfg_multi_unity.cpp
    ${OPENRFG_SRC}/volition/rfg/code/multi_server_browser.cpp
    # stub_impl.md S2: the generated achievement and stat tables of the Steam build
    ${OPENRFG_SRC}/game/build/win32/steam/master/achievements/rfgachievements_adf.cpp
    ${OPENRFG_SRC}/game/build/win32/steam/master/stats/rfgstats_stdf.cpp
)
target_link_libraries(rfg_multi PUBLIC rfg_net rfg platform_discord_rpc)
