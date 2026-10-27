# psxrecomp full runtime + SO2 boot exe C -> psxrecomp-game.exe, MSVC, offline.
# Builds straight from the read-only clone (nothing is written into it): generated files go to the build dir.
# Netplay/lobby (recomp-net) and the debug TCP server are compiled out by options; process spawning, DLL loading and
# all Winsock calls are cut at link time by stubs/sandbox_stubs.c (see that file); build.ps1 audits the import table.
cmake_minimum_required(VERSION 3.24)
project(so2_psxrt C CXX)

set(PSXRECOMP_ROOT "" CACHE PATH "read-only psxrecomp clone (f42d2c0)")
set(GAME_GEN_DIR "" CACHE PATH "generator output dir with SCUS_944.21_full_NN.c + _dispatch.c")
set(DEPS_PREFIX "" CACHE PATH "SDL3 + zlib install prefix from build_deps.ps1")
foreach(v PSXRECOMP_ROOT GAME_GEN_DIR DEPS_PREFIX)
    if(NOT EXISTS "${${v}}")
        message(FATAL_ERROR "${v} not set or missing")
    endif()
endforeach()
set(R "${PSXRECOMP_ROOT}")

# Source list = the clone's own PSXRECOMP_RUNTIME_SOURCES block (runtime.cmake), so it stays in sync with the pin.
file(STRINGS "${R}/runtime/runtime.cmake" _lines)
set(_in FALSE)
set(RT_SOURCES "")
foreach(l IN LISTS _lines)
    if(l MATCHES "^set\\(PSXRECOMP_RUNTIME_SOURCES")
        set(_in TRUE)
    elseif(_in AND l MATCHES "^\\)")
        break()
    elseif(_in AND l MATCHES "^[ \t]*\\\${PSXRECOMP_ROOT}/([^ \t]+)")
        list(APPEND RT_SOURCES "${R}/${CMAKE_MATCH_1}")
    endif()
endforeach()
list(REMOVE_DUPLICATES RT_SOURCES)
list(LENGTH RT_SOURCES _n)
if(_n LESS 150)
    message(FATAL_ERROR "runtime source list parse failed (${_n})")
endif()
message(STATUS "runtime sources from runtime.cmake: ${_n}")
# non-oracle build: stub interpreter (runtime.cmake mode_source)
list(APPEND RT_SOURCES "${R}/runtime/src/stub_interpreter.c")

# BIOS backends: OpenBIOS (generated C committed in the clone) plus SCPH5501 when recomp/bios/gen_scph5501.ps1 has
# emitted it from the user's dump into RETAIL_BIOS_GEN_DIR (gitignored). Registry is generated like runtime.cmake does.
set(RETAIL_BIOS_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../bios/_gen/generated" CACHE PATH "SCPH5501_full.c + _dispatch.c")
set(BIOS_STEMS OpenBIOS)
set(BIOS_SOURCES "${R}/generated/OpenBIOS_full.c" "${R}/generated/OpenBIOS_dispatch.c")
if(EXISTS "${RETAIL_BIOS_GEN_DIR}/SCPH5501_full.c" AND EXISTS "${RETAIL_BIOS_GEN_DIR}/SCPH5501_dispatch.c")
    list(APPEND BIOS_STEMS SCPH5501)
    list(APPEND BIOS_SOURCES "${RETAIL_BIOS_GEN_DIR}/SCPH5501_full.c" "${RETAIL_BIOS_GEN_DIR}/SCPH5501_dispatch.c")
endif()
message(STATUS "BIOS backends linked: ${BIOS_STEMS}")
set(GEN_INC "${CMAKE_BINARY_DIR}/gen_include")
file(MAKE_DIRECTORY "${GEN_INC}")
set(_ext "")
set(_ent "")
foreach(s IN LISTS BIOS_STEMS)
    string(APPEND _ext "extern const PsxBiosBackend ${s}_psx_bios_backend;\n")
    string(APPEND _ent "    &${s}_psx_bios_backend,\n")
endforeach()
list(LENGTH BIOS_STEMS _nb)
file(WRITE "${CMAKE_BINARY_DIR}/psx_bios_registry.c"
"#include \"psx_bios_backend.h\"\n${_ext}"
"const PsxBiosBackend *const psx_bios_builtin_registry[] = {\n${_ent}};\n"
"const uint32_t psx_bios_builtin_registry_count = ${_nb}u;\n")
list(APPEND BIOS_SOURCES "${CMAKE_BINARY_DIR}/psx_bios_registry.c")
# runtime.cmake: first non-OpenBIOS stem is the expected retail image; the 5501 shell has no seeds (PSX_SHELLWIN_INTERP=ON)
if("SCPH5501" IN_LIST BIOS_STEMS)
    set(BIOS_DEFS PSX_EXPECTED_BIOS_STEM="SCPH5501" PSX_SETUP_BIOS_STEMS="OpenBIOS|SCPH5501" PSX_SHELLWIN_INTERP_DEFAULT=1)
else()
    set(BIOS_DEFS PSX_EXPECTED_BIOS_STEM="" PSX_SETUP_BIOS_STEMS="OpenBIOS")
endif()

# overlay_codegen_hash.h: runtime.cmake writes it into the clone; we write it into the build dir instead.
set(PSXRECOMP_CODEGEN_HASH_ROOT "${R}")
include("${R}/runtime/codegen_hash_sources.cmake")
add_custom_command(OUTPUT "${GEN_INC}/overlay_codegen_hash.h"
    COMMAND ${CMAKE_COMMAND} "-DOUT=${GEN_INC}/overlay_codegen_hash.h" "-DSRCS=${PSXRECOMP_CODEGEN_HASH_SRCS}"
            -P "${R}/runtime/hash_codegen.cmake"
    DEPENDS ${PSXRECOMP_CODEGEN_HASH_SRCS} VERBATIM)
add_custom_target(codegen_hash DEPENDS "${GEN_INC}/overlay_codegen_hash.h")

file(GLOB GAME_SOURCES "${GAME_GEN_DIR}/SCUS_944.21_full_*.c")
list(APPEND GAME_SOURCES "${GAME_GEN_DIR}/SCUS_944.21_dispatch.c")

# AOT overlay C (aot_overlay_pipeline.py `static`, recomp/aot/run_static_all.py): one dispatcher overlays_static.c plus
# its overlays_static_NNNN.c units. The runtime needs no registration: PSX_HAS_OVERLAY_DISPATCH makes the dirty-RAM
# interpreter ask psx_overlay_dispatch() first, which picks a variant by the CRC of the bytes now in RAM.
set(OVERLAY_STATIC_DIR "" CACHE PATH "dir with overlays_static.c + overlays_static_NNNN.c (empty: overlays interpreted)")
set(OVERLAY_SOURCES "")
if(OVERLAY_STATIC_DIR)
    if(NOT EXISTS "${OVERLAY_STATIC_DIR}/overlays_static.c")
        message(FATAL_ERROR "no overlays_static.c in OVERLAY_STATIC_DIR=${OVERLAY_STATIC_DIR}")
    endif()
    file(GLOB OVERLAY_SOURCES "${OVERLAY_STATIC_DIR}/overlays_static*.c")
    list(LENGTH OVERLAY_SOURCES _ov_n)
    message(STATUS "AOT overlay sources: ${_ov_n} from ${OVERLAY_STATIC_DIR}")
    add_compile_definitions(PSX_HAS_OVERLAY_DISPATCH=1)
endif()
set(OVERLAY_OPT "/Od" CACHE STRING "cl optimization flag for the overlay C")

# Copy-with-patch: a few TUs need a minimal edit for MSVC or for the sandbox. The edited copy goes to the build dir
# (the clone is never touched), replaces the original in the source list, and each edit must match exactly once.
set(PATCHED "${CMAKE_BINARY_DIR}/patched")
function(psxrt_patch rel)   # psxrt_patch(<path under clone> OLD1 NEW1 [OLD2 NEW2 ...]); no ';' in OLD/NEW (CMake lists)
    file(READ "${R}/${rel}" txt)
    set(args ${ARGN})
    list(LENGTH args n)
    math(EXPR last "${n} - 1")
    foreach(i RANGE 0 ${last} 2)
        math(EXPR j "${i} + 1")
        list(GET args ${i} old)
        list(GET args ${j} new)
        string(FIND "${txt}" "${old}" p1)
        string(FIND "${txt}" "${old}" p2 REVERSE)
        if(p1 EQUAL -1 OR NOT p1 EQUAL p2)
            message(FATAL_ERROR "patch for ${rel} does not match exactly once: ${old}")
        endif()
        string(REPLACE "${old}" "${new}" txt "${txt}")
    endforeach()
    get_filename_component(name "${rel}" NAME)
    file(WRITE "${PATCHED}/${name}" "${txt}")
    list(REMOVE_ITEM RT_SOURCES "${R}/${rel}")
    list(APPEND RT_SOURCES "${PATCHED}/${name}")
    set(RT_SOURCES "${RT_SOURCES}" PARENT_SCOPE)
endfunction()
# MSVC 19.44 rejects a constexpr local named inside a [&] lambda in static_assert (C2131); clang/gcc accept it.
# Both flags are constexpr false a few thousand lines above, so the assert is a no-op either way.
psxrt_patch(runtime/src/main.cpp
    "static_assert(!frame_interpolation_offered && !skip_fmv_offered,"
    "static_assert(true, /* psxrt: MSVC C2131 */"
    "static void env_pad_hold_apply(void) {"
    "static void env_pad_hold_apply(void) {\n    if ((psxrt_input_script_apply(), false)) {} /* psxrt: scripted pad, stubs/psxrt_io.c */")
# Audio evidence tap (stubs/psxrt_io.c, off unless PSXRT_AUDIO_RMS=1): sees the exact bytes handed to the SDL3 stream.
psxrt_patch(runtime/src/psx_sdl_audio.cpp
    "        s_audio_callback(s_audio_userdata, buffer, chunk)"
    "        s_audio_callback(s_audio_userdata, buffer, chunk), psxrt_audio_tap(buffer, chunk)")
set_property(SOURCE "${PATCHED}/main.cpp" "${PATCHED}/psx_sdl_audio.cpp" APPEND PROPERTY COMPILE_OPTIONS
    "/FI${CMAKE_CURRENT_SOURCE_DIR}/stubs/psxrt_io.h")
# GCC __atomic builtins, unguarded only here (every other use has an _WIN32/_MSC_VER branch); int-sized, so the
# MSVC Interlocked intrinsics give the same (stronger, seq-cst) ordering.
psxrt_patch(runtime/src/gpu_gl_renderer.c
    "static int s_pgxp_render_wanted = 0"
    "#include <intrin.h> /* psxrt: MSVC has no __atomic builtins */\n#define psxrt_store_rel(p, v, o) _InterlockedExchange((volatile long *)(p), (long)(v))\nstatic int s_pgxp_render_wanted = 0"
    "__atomic_store_n(&s_pgxp_render_wanted,"
    "psxrt_store_rel(&s_pgxp_render_wanted,"
    "__ATOMIC_RELEASE)"
    "0)"
    "__atomic_load_n(&s_pgxp_render_wanted, __ATOMIC_ACQUIRE)"
    "_InterlockedOr((volatile long *)&s_pgxp_render_wanted, 0)")

# libwebp is not built: replace the decoder TU with a stub (HD texture packs off)
list(REMOVE_ITEM RT_SOURCES "${R}/runtime/src/texture_image_decode.cpp")

# generated C is tens of MB: /Od keeps the compile time sane (same flag as recomp/host/compile_generated.ps1)
set(_bios_full ${BIOS_SOURCES})
list(FILTER _bios_full INCLUDE REGEX "_full\\.c$")
set_property(SOURCE ${GAME_SOURCES} ${_bios_full} APPEND PROPERTY COMPILE_OPTIONS /Od)
if(OVERLAY_SOURCES)
    set_property(SOURCE ${OVERLAY_SOURCES} APPEND PROPERTY COMPILE_OPTIONS ${OVERLAY_OPT})
endif()

add_executable(psxrecomp-game
    ${RT_SOURCES} ${BIOS_SOURCES} ${GAME_SOURCES} ${OVERLAY_SOURCES} stubs/psxrt_ovstats.c
    "${R}/runtime/src/psx_sha256_file.cpp"   # runtime.cmake adds it via psxrecomp_sha256_file()
    stubs/sandbox_stubs.c stubs/texture_image_decode_stub.cpp stubs/psxrt_diag.c stubs/psxrt_bios_probe.c stubs/psxrt_io.c)
add_dependencies(psxrecomp-game codegen_hash)
target_compile_features(psxrecomp-game PRIVATE c_std_11 cxx_std_17)
target_include_directories(psxrecomp-game PRIVATE
    "${GEN_INC}" "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
    "${R}/runtime/include" "${R}/runtime/src" "${R}/recompiler/src" "${R}/recompiler/include"
    "${R}/recompiler/lib/fmt/include" "${R}/recompiler/lib/toml11"
    "${GAME_GEN_DIR}" "${DEPS_PREFIX}/include")
target_compile_definitions(psxrecomp-game PRIVATE
    DEFAULT_DEBUG_PORT=0
    PSX_DEFAULT_BIOS_PATH="bios/OpenBIOS.BIN"
    ${BIOS_DEFS}
    PSX_SETUP_FRAMEWORK_REL="psxrecomp"
    PSX_BUNDLED_BIOS_PATH="bios/openbios.bin"
    PSX_DEFAULT_GAME_CONFIG_PATH="game.toml"
    PSX_WINDOW_TITLE="Star Ocean The Second Story"
    PSX_MAX_PLAYERS=2
    PSX_MEMCARD_SLOTS=2
    PSX_GAME_VERSION="dev"
    FMT_HEADER_ONLY=1 NOMINMAX PSX_SDL3=1 SDL_MAIN_HANDLED
    PSX_NO_DEBUG_TOOLS=1
    PSX_HAS_GAME_DISPATCH=1
    PSX_NATIVE_BUILD=1 PSX_ENABLE_BLOCK_CYCLES=1)   # runtime.cmake non-oracle build: without these no guest cycles are counted
# execution identity header (profile.h + .execution.json), written into our build dir
include("${R}/cmake/psx_execution_profile.cmake")
psxrecomp_execution_profile(psxrecomp-game CONTRACT_FILES
    "${R}/runtime/src/gte.cpp" "${R}/runtime/src/pgxp.cpp" "${R}/runtime/include/hle_gte.h"
    "${R}/runtime/include/cpu_state.h" "${R}/runtime/include/gte.h" "${R}/runtime/include/pgxp.h"
    "${R}/runtime/include/render_pass_projection.h")
set_property(SOURCE "${R}/runtime/src/game_dispatch_compat.c" APPEND PROPERTY COMPILE_DEFINITIONS
    PSX_GAME_DISPATCH_HAS_NATIVE_OK=1 PSX_GAME_DISPATCH_HAS_NATIVE_OK_FULL=1)
target_compile_options(psxrecomp-game PRIVATE $<$<COMPILE_LANGUAGE:C>:/experimental:c11atomics> /bigobj /utf-8 /GS- /guard:cf- /MP /wd4102 /wd4244 /wd4267 /wd4996 /wd4146)
set_property(TARGET psxrecomp-game PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")   # static CRT: its spawn paths go through our stubs
target_link_directories(psxrecomp-game PRIVATE "${DEPS_PREFIX}/lib")
target_link_libraries(psxrecomp-game PRIVATE SDL3.lib zlibstatic.lib dbghelp comdlg32 opengl32 shell32 bcrypt)
# console subsystem (headless logs); 64 MB stack as runtime.cmake; never take ws2_32/iphlpapi from a #pragma comment(lib)
target_link_options(psxrecomp-game PRIVATE /STACK:67108864,67108864 /GUARD:NO
    /NODEFAULTLIB:ws2_32.lib /NODEFAULTLIB:iphlpapi.lib /NODEFAULTLIB:wsock32.lib /NODEFAULTLIB:wininet.lib /NODEFAULTLIB:winhttp.lib)
