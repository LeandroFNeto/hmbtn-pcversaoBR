# Native host executable. Selected only when the root list is configured with
# -DHMBTN_HOST=ON. The PS1 toolchain is not loaded on this path.

set(HMBTN_GAME_INCLUDE_DIRS
    src/include
    src/main src/main/cd src/main/game src/main/geometry src/main/graphics src/main/input
    src/main/sound src/main/text src/main/ui
    src/overlay/11_GF_FARM src/overlay/11_GF_FARM/animals src/overlay/11_GF_FARM/event
    src/overlay/11_GF_FARM/field src/overlay/11_GF_FARM/menu src/overlay/11_GF_FARM/people
    src/overlay/11_GF_FARM/world
    src/overlay/08_DEBUG src/overlay/10_PdaSamp src/overlay/15_saveload
    src/overlay/17_GF_MG1 src/overlay/19_GF_MG2 src/overlay/21_GF_MG3 src/overlay/23_GF_MG4
    src/overlay/25_GF_MG5 src/overlay/27_GF_TITLE src/overlay/28_GF_STAFF src/overlay/30_GF_swind
    src/overlay/31_gf_mcard
    src)
list(TRANSFORM HMBTN_GAME_INCLUDE_DIRS PREPEND "${CMAKE_SOURCE_DIR}/")

find_package(SDL2 REQUIRED)

set(CMAKE_C_EXTENSIONS ON)

# The game's CMakeLists call psx_exe() and psx_overlay(). On the host those only collect
# the SOURCES, so the host links the same translation units as the PS1 images.
function(_hmbtn_collect_sources)
    foreach(source IN LISTS ARGN)
        cmake_path(ABSOLUTE_PATH source BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                   NORMALIZE OUTPUT_VARIABLE absolute)
        set_property(GLOBAL APPEND PROPERTY HMBTN_GAME_SOURCES ${absolute})
    endforeach()
endfunction()

function(psx_exe name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG ""
        "BASE;STACK;REGION_TEXT;GP;SDK_OBJECTS;SDK_PATCHES" "SOURCES;SMALL_COMMONS")
    _hmbtn_collect_sources(${ARG_SOURCES})
endfunction()

function(psx_overlay name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "OWN_COMMONS" "FILE;REGION" "SOURCES;GAP")
    _hmbtn_collect_sources(${ARG_SOURCES})
endfunction()

foreach(dir IN ITEMS
        src/main
        src/overlay/08_DEBUG src/overlay/10_PdaSamp src/overlay/11_GF_FARM
        src/overlay/15_saveload src/overlay/17_GF_MG1 src/overlay/19_GF_MG2
        src/overlay/21_GF_MG3 src/overlay/23_GF_MG4 src/overlay/25_GF_MG5
        src/overlay/27_GF_TITLE src/overlay/28_GF_STAFF src/overlay/30_GF_swind
        src/overlay/31_gf_mcard)
    add_subdirectory(${dir})
endforeach()
get_property(HMBTN_GAME_SOURCES GLOBAL PROPERTY HMBTN_GAME_SOURCES)

# GCC 2.95 accepted constructs modern GCC rejects or reads differently, with no flag to
# restore the old meaning. For those files the host compiles a copy under the build tree
# with textual replacements, given as `from to` pairs and applied in order, each to every
# occurrence; the file under src/ is never edited. The texts cannot hold a ';' (a CMake
# list separator).
function(_hmbtn_write_patched relative)
    set(original ${CMAKE_SOURCE_DIR}/${relative})
    set(patched ${CMAKE_BINARY_DIR}/host_patched/${relative})
    list(LENGTH ARGN count)
    math(EXPR odd "${count} % 2")
    if(count EQUAL 0 OR odd)
        message(FATAL_ERROR "hmbtn_host_patch: ${relative} needs `from to` pairs")
    endif()
    file(READ ${original} contents)
    math(EXPR last "${count} - 2")
    foreach(i RANGE 0 ${last} 2)
        math(EXPR j "${i} + 1")
        list(GET ARGN ${i} from)
        list(GET ARGN ${j} to)
        string(FIND "${contents}" "${from}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "hmbtn_host_patch: '${from}' not found in ${relative}")
        endif()
        string(REPLACE "${from}" "${to}" contents "${contents}")
    endforeach()
    file(WRITE ${patched}.tmp "${contents}")
    configure_file(${patched}.tmp ${patched} COPYONLY)
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${original})
endfunction()

function(hmbtn_host_patch relative)
    _hmbtn_write_patched(${relative} ${ARGN})
    list(TRANSFORM HMBTN_GAME_SOURCES REPLACE "^${CMAKE_SOURCE_DIR}/${relative}$"
         "${CMAKE_BINARY_DIR}/host_patched/${relative}")
    set(HMBTN_GAME_SOURCES ${HMBTN_GAME_SOURCES} PARENT_SCOPE)
endfunction()

# The patched header's directory is searched before the game's, except by sources in the
# header's own directory, whose quote includes find the original first.
function(hmbtn_host_patch_header relative)
    _hmbtn_write_patched(${relative} ${ARGN})
    get_filename_component(dir ${CMAKE_BINARY_DIR}/host_patched/${relative} DIRECTORY)
    list(APPEND HMBTN_PATCHED_INCLUDE_DIRS ${dir})
    list(REMOVE_DUPLICATES HMBTN_PATCHED_INCLUDE_DIRS)
    set(HMBTN_PATCHED_INCLUDE_DIRS ${HMBTN_PATCHED_INCLUDE_DIRS} PARENT_SCOPE)
endfunction()

# text.h declares these extern; text.c redefines them static.
hmbtn_host_patch(src/main/text/text.c "static char text_fmt_" "char text_fmt_")

# The object dump reads the frustum planes and the view point. The functions that
# write them stay as they are; only the static keyword is lifted so the host can see them.
hmbtn_host_patch(src/main/geometry/model.c
    "static ClipPlane clip_planes[4];" "ClipPlane clip_planes[4];"
    "static VECTOR view_direction;" "VECTOR view_direction;")

# GCC 2.95 stored the static initializers of a zero-length array after the struct
# (farm_data.c's tables of contents); modern GCC drops them. A flexible array member keeps
# them with the same layout.
set(HMBTN_PATCHED_INCLUDE_DIRS "")
hmbtn_host_patch_header(src/main/graphics/anim.h "s32 start[0];" "s32 start[];")

# A new game's first farm frame reads the area's place before scene_enter sets
# farm_area.mode.data, and an area without marked models leaves scene_marked_entries
# null while their flags are updated. The PS1 reaches its kernel RAM at address 0 there
# (the write at 0x28 lands in the unused area) and goes on.
hmbtn_host_patch(src/overlay/11_GF_FARM/world/scene.c
    "#include \"scene.h\"" "#include \"scene.h\"\n#include <ps1_null.h>"
    "farm_area.mode.data->head.place" "PS1_NULL_PTR(farm_area.mode.data)->head.place"
    "scene_marked_entries[0]->" "PS1_NULL_PTR(scene_marked_entries[0])->"
    "scene_marked_entries[1]->" "PS1_NULL_PTR(scene_marked_entries[1])->")

# A ground probe that finds nothing leaves ground_hit_primitive() null; these readers do
# not test it, and on the PS1 they read the kernel's word 3 at address 0 ("no ground").
# farm_probe.c's two readers that do test for null are left alone.
set(hmbtn_null_include "#include \"types.h\"" "#include \"types.h\"\n#include <ps1_null.h>")
hmbtn_host_patch(src/overlay/11_GF_FARM/people/player.c ${hmbtn_null_include}
    "ground_hit_primitive()" "PS1_NULL_PTR(ground_hit_primitive())")
hmbtn_host_patch(src/overlay/11_GF_FARM/farm_probe.c ${hmbtn_null_include}
    "*(u8 *)ground_hit_primitive()" "*(u8 *)PS1_NULL_PTR(ground_hit_primitive())")
hmbtn_host_patch(src/overlay/11_GF_FARM/field/field.c ${hmbtn_null_include}
    "hit->header & 8" "PS1_NULL_PTR(hit)->header & 8")
hmbtn_host_patch(src/overlay/11_GF_FARM/field/greenhouse.c ${hmbtn_null_include}
    "hit->header & FARM_QUAD_CELL_BIT" "PS1_NULL_PTR(hit)->header & FARM_QUAD_CELL_BIT")
hmbtn_host_patch(src/overlay/11_GF_FARM/field/mine.c ${hmbtn_null_include}
    "hit->header & FARM_QUAD_CELL_BIT" "PS1_NULL_PTR(hit)->header & FARM_QUAD_CELL_BIT")

# harvest_helper_timer_tick calls the cue with an empty argument list. On the PS1 the
# helper is still in a0. On x86 that leaves self null and taskId is read at address 0x2A.
hmbtn_host_patch(src/overlay/11_GF_FARM/people/harvest_sprites.c
    "        action();" "        action(helper);")

file(GLOB HMBTN_PORT_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/pc_port/src/*.c)
set_source_files_properties(${CMAKE_SOURCE_DIR}/src/pc_port/src/objects_dump.c PROPERTIES COMPILE_FLAGS -Os)
set_source_files_properties(${CMAKE_SOURCE_DIR}/src/pc_port/src/tmd_vram.c PROPERTIES COMPILE_FLAGS -Os)

add_executable(hmbtn-host ${HMBTN_PORT_SOURCES} ${HMBTN_GAME_SOURCES})

# Every compile goes through host_cc_check.cmake, which fails it if GCC dropped array
# initializers that GCC 2.95 kept.
set_target_properties(hmbtn-host PROPERTIES C_COMPILER_LAUNCHER
    "${CMAKE_COMMAND};-P;${CMAKE_SOURCE_DIR}/cmake/host_cc_check.cmake;--")

target_include_directories(hmbtn-host BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/src/pc_port/include
    ${HMBTN_PATCHED_INCLUDE_DIRS}
    ${HMBTN_GAME_INCLUDE_DIRS})

# The PS1 build passes -D__CHAR_UNSIGNED__; tentative definitions shared across
# overlays rely on common symbols.
target_compile_options(hmbtn-host PRIVATE -funsigned-char -fcommon -g)
# libgte's csqrt is the PS1's integer square root, not C99's complex one.
target_compile_options(hmbtn-host PRIVATE -fno-builtin-csqrt)

# Renames the decomp's main() to ps1_main in every game translation unit.
# main_pc.c undefines the macro so the PC entry point stays main().
target_compile_definitions(hmbtn-host PRIVATE main=ps1_main)

# Off: the host loop calls VSync(0) and the game is linked but not entered.
option(HMBTN_RUN_GAME "Call ps1_main from the host entry" OFF)
set_property(SOURCE ${CMAKE_SOURCE_DIR}/src/pc_port/src/main_pc.c APPEND PROPERTY
    COMPILE_DEFINITIONS HMBTN_RUN_GAME=$<BOOL:${HMBTN_RUN_GAME}>)

target_link_libraries(hmbtn-host PRIVATE SDL2::SDL2)
if(WIN32)
    target_link_libraries(hmbtn-host PRIVATE opengl32)
endif()
if(MINGW)
    # Keep the image in the low 16MB so a static packet address survives DrawOTag's 24-bit link.
    target_link_options(hmbtn-host PRIVATE "-Wl,--image-base,0x400000")
    # Lets the 32-bit process map the PS1 RAM window at 0x80000000 on 64-bit Windows.
    target_link_options(hmbtn-host PRIVATE "-Wl,--large-address-aware")
    add_custom_command(TARGET hmbtn-host POST_BUILD
        COMMAND ${CMAKE_COMMAND} -DIMAGE=$<TARGET_FILE:hmbtn-host>
            -P ${CMAKE_SOURCE_DIR}/cmake/host_image_check.cmake
        VERBATIM)
    # The PS1 linker script places the overlay load regions; the host binds the same addresses.
    target_link_options(hmbtn-host PRIVATE
        "-Wl,--defsym,_overlay_low=0x800B7E20"
        "-Wl,--defsym,_overlay_high=0x8012DDE8")
endif()

add_custom_command(TARGET hmbtn-host POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL2::SDL2>
        $<TARGET_FILE_DIR:hmbtn-host>)

# The disc's files, extracted once from the original image into disc/ beside the
# executable, where the host CD drive reads them.
set(ORIGINAL_DISC "" CACHE FILEPATH
    "The original disc image (its .bin, MODE2/2352) the host CD drive serves")
set(hmbtn_disc_image ${ORIGINAL_DISC})
if(NOT hmbtn_disc_image AND EXISTS ${CMAKE_SOURCE_DIR}/baserom.bin)
    set(hmbtn_disc_image ${CMAKE_SOURCE_DIR}/baserom.bin)
    message(STATUS "ORIGINAL_DISC is not set: using ${hmbtn_disc_image}")
endif()
if(hmbtn_disc_image)
    if(NOT EXISTS ${hmbtn_disc_image})
        message(FATAL_ERROR "ORIGINAL_DISC: ${hmbtn_disc_image} does not exist")
    endif()
    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
    set(hmbtn_disc_dir ${CMAKE_BINARY_DIR}/disc)
    add_custom_command(
        OUTPUT ${hmbtn_disc_dir}.stamp
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${hmbtn_disc_dir}
        COMMAND ${Python3_EXECUTABLE} tools/psx_disc.py extract ${hmbtn_disc_image} --out ${hmbtn_disc_dir}
        COMMAND ${CMAKE_COMMAND} -E touch ${hmbtn_disc_dir}.stamp
        DEPENDS ${hmbtn_disc_image} ${CMAKE_SOURCE_DIR}/tools/psx_disc.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Extracting the disc's files from ${hmbtn_disc_image}"
        VERBATIM)
    add_custom_target(hmbtn_disc DEPENDS ${hmbtn_disc_dir}.stamp)
    add_dependencies(hmbtn-host hmbtn_disc)
else()
    message(STATUS "No disc image: the host CD drive has no files (set ORIGINAL_DISC)")
endif()
