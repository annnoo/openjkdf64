# cmake/toolchain-n64.cmake
# Cross-compilation toolchain for Nintendo 64 (libdragon / mips64-elf)
#
# Paths are as they appear INSIDE the libdragon Docker container.
# The build script mounts the project into the container and runs cmake there,
# so these absolute paths resolve correctly at configure + compile time.

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR mips)

# ── Toolchain location (inside container) ─────────────────────────────────────
if(DEFINED ENV{N64_INST})
    set(N64_INST $ENV{N64_INST})
else()
    set(N64_INST /n64_toolchain)
endif()

set(MIPS_PREFIX ${N64_INST}/bin/mips64-elf-)

set(CMAKE_C_COMPILER   ${MIPS_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${MIPS_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${MIPS_PREFIX}gcc)
set(CMAKE_AR           ${MIPS_PREFIX}ar   CACHE FILEPATH "")
set(CMAKE_RANLIB       ${MIPS_PREFIX}ranlib CACHE FILEPATH "")
set(CMAKE_OBJCOPY      ${MIPS_PREFIX}objcopy CACHE FILEPATH "")
set(CMAKE_SIZE         ${MIPS_PREFIX}size    CACHE FILEPATH "")
set(CMAKE_STRIP        ${MIPS_PREFIX}strip   CACHE FILEPATH "")

# ── Sysroot / search paths ────────────────────────────────────────────────────
set(CMAKE_SYSROOT ${N64_INST}/mips64-elf)

set(CMAKE_FIND_ROOT_PATH        ${N64_INST}/mips64-elf)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ── Don't try to execute cross-compiled test binaries ─────────────────────────
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# ── EXE suffix (none — we produce .elf ourselves) ─────────────────────────────
set(CMAKE_EXECUTABLE_SUFFIX "")
