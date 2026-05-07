# cmake_modules/plat_n64.cmake
# Platform module for Nintendo 64 (libdragon)
#
# Toolchain: cmake/toolchain-n64.cmake   (cross-compiler setup)
# This file: compile/link flags, libdragon include/libs, .z64 ROM packaging.

macro(plat_initialize)
    message(STATUS "Targeting Nintendo 64 (libdragon)")

    set(BIN_NAME "openjkdf2.elf")
    set(N64_ROM_TITLE "OpenJKDF2")

    if(DEFINED ENV{N64_INST})
        set(N64_INST $ENV{N64_INST})
    else()
        set(N64_INST /n64_toolchain)
    endif()

    # ── Preprocessor defines ───────────────────────────────────────────────────
    add_definitions(
        -DPLAT_MISSING_WIN32
        -DTARGET_N64
        -DN64
        -D_XOPEN_SOURCE=500
        -D_DEFAULT_SOURCE
        -DCOG_USE_PRECOMPILED
        -DSTDSOUND_NULL
        -DPLATFORM_POSIX
        -DNO_JK_MMAP
    )

    # ── Feature flags (all OFF for minimal N64 build) ─────────────────────────
    set(TARGET_USE_PHYSFS                FALSE)
    set(TARGET_USE_GAMENETWORKINGSOCKETS FALSE)
    set(TARGET_USE_LIBSMACKER           FALSE)
    set(TARGET_USE_LIBSMUSHER           FALSE)
    set(TARGET_USE_SDL2                 FALSE)
    set(TARGET_USE_OPENGL               FALSE)
    set(TARGET_USE_OPENAL               FALSE)
    set(TARGET_POSIX                    TRUE)
    set(TARGET_NO_BLOBS                 TRUE)
    set(TARGET_CAN_JKGM                 FALSE)
    set(OPENJKDF2_NO_ASAN               TRUE)
    set(TARGET_USE_CURL                 FALSE)
    set(TARGET_FIND_OPENAL              FALSE)
    set(TARGET_NO_MULTIPLAYER_MENUS     TRUE)
    set(TARGET_BUILD_TESTS              FALSE)
    set(SDL2_COMMON_LIBS                "")
    set(TARGET_N64                      TRUE)

    # ── Compile flags (must match n64.mk / libdragon container settings) ───────
    # n64.mk uses -mabi=o64; the old cmake stub had -mabi=32 which is wrong.
    if(N64_DEBUG)
        message(STATUS "N64 DEBUG build: -g -O0")
        set(N64_OPT_FLAGS -O0 -g)
        set(N64_FRAME_PTR "")   # keep frame pointers for GDB stack traces
        add_definitions(-DN64_DEBUG_BUILD)
    else()
        set(N64_OPT_FLAGS -Os)
        set(N64_FRAME_PTR -fomit-frame-pointer)
    endif()

    add_compile_options(
        -march=vr4300
        -mtune=vr4300
        -mabi=o64
        -ffunction-sections
        -fdata-sections
        ${N64_FRAME_PTR}
        -ffast-math
        -ftrapping-math
        -fno-associative-math
        -ftrivial-auto-var-init=pattern
        ${N64_OPT_FLAGS}
        -fshort-wchar
        -Wall
        -Wno-unused-variable
        -Wno-parentheses
        -Wno-missing-braces
        -Wno-incompatible-pointer-types
        -Wno-error=deprecated-declarations
    )
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -std=gnu17")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++17")

    add_link_options(
        -march=vr4300
        -mtune=vr4300
        -mabi=o64
        -Wl,--gc-sections
    )

    # ── libdragon headers ──────────────────────────────────────────────────────
    include_directories(
        ${N64_INST}/mips64-elf/include
        ${N64_INST}/mips64-elf/include/newlib_overrides
    )
    # Force-include ktls.h the same way n64.mk does
    add_compile_options(-include ktls.h)
endmacro()

macro(plat_specific_deps)
    set(SDL2_COMMON_LIBS "")
endmacro()

macro(plat_link_and_package)
    if(DEFINED ENV{N64_INST})
        set(N64_INST $ENV{N64_INST})
    else()
        set(N64_INST /n64_toolchain)
    endif()

    set(N64_TOOLS      ${N64_INST}/bin)
    set(N64_LD_SCRIPT  ${N64_INST}/mips64-elf/lib/n64.ld)
    set(LIBDRAGON_A    ${N64_INST}/mips64-elf/lib/libdragon.a)
    set(LIBDRAGONSYS_A ${N64_INST}/mips64-elf/lib/libdragonsys.a)

    set(ELF_FILE       ${CMAKE_BINARY_DIR}/${BIN_NAME})
    set(ELF_STRIPPED   ${CMAKE_BINARY_DIR}/openjkdf2.elf.stripped)
    set(ELF_SYM        ${CMAKE_BINARY_DIR}/openjkdf2.elf.sym)
    set(ROM_TMP        ${CMAKE_BINARY_DIR}/openjkdf2.z64.tmp)
    set(ROM_OUT        ${CMAKE_BINARY_DIR}/openjkdf2.z64)
    set(DFS_OUT        ${CMAKE_BINARY_DIR}/openjkdf2.dfs)
    set(FILESYSTEM_DIR ${CMAKE_SOURCE_DIR}/filesystem)

    # ── Link ───────────────────────────────────────────────────────────────────
    target_link_libraries(${BIN_NAME} PRIVATE
        ${LIBDRAGON_A}
        ${LIBDRAGONSYS_A}
        -lm -lc
    )
    target_link_libraries(sith_engine PRIVATE nlohmann_json::nlohmann_json)

    target_link_options(${BIN_NAME} PRIVATE
        -Wl,-g
        -Wl,-L${N64_INST}/mips64-elf/lib
        -Wl,-Tn64.ld
        -Wl,--wrap -Wl,__do_global_ctors
        -Wl,-Map,${CMAKE_BINARY_DIR}/openjkdf2.map,--cref
    )

    # ── Post-link: package ELF → .z64 ─────────────────────────────────────────
    # Mirrors exactly what n64.mk does after linking.
    add_custom_command(TARGET ${BIN_NAME} POST_BUILD
        # 1. Generate symbol file
        COMMAND ${N64_TOOLS}/n64sym --all ${ELF_FILE} ${ELF_SYM}

        # 2. Strip binary
        COMMAND ${CMAKE_COMMAND} -E copy ${ELF_FILE} ${ELF_STRIPPED}
        COMMAND ${N64_TOOLS}/mips64-elf-strip -s ${ELF_STRIPPED}

        # 3. Compress ELF
        COMMAND ${N64_TOOLS}/n64elfcompress -o ${CMAKE_BINARY_DIR}/ -c 1 ${ELF_STRIPPED}

        # 4. Build DFS filesystem image from filesystem/ dir
        COMMAND ${CMAKE_COMMAND} -E make_directory ${FILESYSTEM_DIR}
        COMMAND ${N64_TOOLS}/mkdfs ${DFS_OUT} ${FILESYSTEM_DIR}

        # 5. Pack ROM (ELF + sym + DFS)
        COMMAND ${N64_TOOLS}/n64tool
            --toc
            --title "${N64_ROM_TITLE}"
            --output ${ROM_TMP}
            --align 256 ${ELF_STRIPPED}
            ${ELF_SYM}
            ${N64_INST}/mips64-elf/include/libdragon.version
            ${N64_INST}/mips64-elf/include/toolchain.version
            --align 16 ${DFS_OUT}

        # 6. Make region-free
        COMMAND ${N64_TOOLS}/ed64romconfig --regionfree ${ROM_TMP}

        # 7. Rename to final ROM
        COMMAND ${CMAKE_COMMAND} -E rename ${ROM_TMP} ${ROM_OUT}

        COMMENT "Packaging ${ROM_OUT} (with DFS from ${FILESYSTEM_DIR})"
        VERBATIM
    )
endmacro()

macro(plat_extra_deps)
endmacro()
