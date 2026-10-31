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

# GCC 2.95 accepted constructs modern GCC rejects with no flag to relax them. For those
# files the host compiles a copy under the build tree with one textual replacement; the
# file under src/ is never edited.
function(hmbtn_host_patch relative from to)
    set(original ${CMAKE_SOURCE_DIR}/${relative})
    set(patched ${CMAKE_BINARY_DIR}/host_patched/${relative})
    file(READ ${original} contents)
    string(FIND "${contents}" "${from}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "hmbtn_host_patch: '${from}' not found in ${relative}")
    endif()
    string(REPLACE "${from}" "${to}" contents "${contents}")
    file(WRITE ${patched}.tmp "${contents}")
    configure_file(${patched}.tmp ${patched} COPYONLY)
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${original})
    list(TRANSFORM HMBTN_GAME_SOURCES REPLACE "^${original}$" "${patched}")
    set(HMBTN_GAME_SOURCES ${HMBTN_GAME_SOURCES} PARENT_SCOPE)
endfunction()

# text.h declares these extern; text.c redefines them static.
hmbtn_host_patch(src/main/text/text.c "static char text_fmt_" "char text_fmt_")

file(GLOB HMBTN_PORT_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/pc_port/src/*.c)

add_executable(hmbtn-host ${HMBTN_PORT_SOURCES} ${HMBTN_GAME_SOURCES})

target_include_directories(hmbtn-host BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/src/pc_port/include
    ${HMBTN_GAME_INCLUDE_DIRS})

# The PS1 build passes -D__CHAR_UNSIGNED__; tentative definitions shared across
# overlays rely on common symbols.
target_compile_options(hmbtn-host PRIVATE -funsigned-char -fcommon -g)

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
