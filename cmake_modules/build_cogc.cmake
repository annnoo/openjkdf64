# build_cogc.cmake
#
# 1. Builds the cogc host tool (always with native gcc via its own Makefile).
# 2. Runs cogc on every .cog in tools/out/cog/ to produce .bcog files
#    alongside them.  The resulting compile_cogs target is added to ALL so
#    it runs automatically as part of every build.

set(COGC_SRC_DIR ${CMAKE_SOURCE_DIR}/tools/cogc)
set(COGC_BIN     ${COGC_SRC_DIR}/cogc)
set(COGC_COG_DIR ${CMAKE_SOURCE_DIR}/tools/out/cog)

# ---------------------------------------------------------------------------
# Step 1: build cogc itself
# ---------------------------------------------------------------------------
add_custom_command(
    OUTPUT  ${COGC_BIN}
    COMMAND make -C ${COGC_SRC_DIR}
    DEPENDS ${COGC_SRC_DIR}/main.c
            ${COGC_SRC_DIR}/cogc_verbs.c
            ${COGC_SRC_DIR}/cogc_libc.c
            ${COGC_SRC_DIR}/bcog.h
            ${COGC_SRC_DIR}/globals.h
            ${COGC_SRC_DIR}/Makefile
    COMMENT "Building cogc host tool"
    VERBATIM
)

add_custom_target(cogc_build DEPENDS ${COGC_BIN})

# ---------------------------------------------------------------------------
# Step 2: compile every .cog -> .bcog
# ---------------------------------------------------------------------------
file(GLOB COG_FILES "${COGC_COG_DIR}/*.cog")

set(BCOG_FILES)
foreach(COG_FILE ${COG_FILES})
    string(REGEX REPLACE "\\.cog$" ".bcog" BCOG_FILE "${COG_FILE}")
    get_filename_component(COG_NAME "${COG_FILE}" NAME)
    add_custom_command(
        OUTPUT  "${BCOG_FILE}"
        COMMAND "${COGC_BIN}" "${COG_FILE}" "${BCOG_FILE}"
        DEPENDS "${COGC_BIN}" "${COG_FILE}"
        COMMENT "cogc: ${COG_NAME}"
        VERBATIM
    )
    list(APPEND BCOG_FILES "${BCOG_FILE}")
endforeach()

add_custom_target(compile_cogs ALL DEPENDS ${BCOG_FILES})
add_dependencies(compile_cogs cogc_build)
