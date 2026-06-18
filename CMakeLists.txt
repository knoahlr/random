cmake_minimum_required(VERSION 3.20)

project(random C ASM)

# ---------------------------------------------------------------------------
# SDK locations. Override on the command line or in CMakePresets.json, e.g.
#   cmake -B build --preset arm-gcc -DTIRTOS_INSTALL=/mnt/c/ti/tirtos_tivac_2_16_00_08
# ---------------------------------------------------------------------------
set(TI_ROOT          "/mnt/c/ti"                                  CACHE PATH "TI tools root")
set(TIRTOS_INSTALL   "${TI_ROOT}/tirtos_tivac_2_16_00_08"         CACHE PATH "TI-RTOS install")
set(TIVAWARE_INSTALL "${TI_ROOT}/TivaWare_C_Series-2.2.0.295"     CACHE PATH "TivaWare install")
set(GEN_DIR          "${CMAKE_SOURCE_DIR}/generated"              CACHE PATH "configuro output (run tools/generate-bios-config.sh first)")

if(NOT EXISTS "${GEN_DIR}")
    message(FATAL_ERROR
        "Generated BIOS config not found at ${GEN_DIR}.\n"
        "Run tools/generate-bios-config.sh once before configuring CMake.")
endif()

# ---------------------------------------------------------------------------
# Harvest the exact include paths (-I) and defines (-D) configuro emitted.
# This keeps the app compile consistent with how the BIOS config was built,
# and is resilient to target-specific xdc_target_* defines.
# ---------------------------------------------------------------------------
set(XDC_INCLUDES "")
set(XDC_DEFINES  "")
file(GLOB _compiler_opt "${GEN_DIR}/compiler.opt" "${GEN_DIR}/*/compiler.opt")
if(_compiler_opt)
    file(READ "${_compiler_opt}" _opt_contents)
    separate_arguments(_opt_tokens UNIX_COMMAND "${_opt_contents}")
    foreach(tok ${_opt_tokens})
        if(tok MATCHES "^-I(.+)$")
            list(APPEND XDC_INCLUDES "${CMAKE_MATCH_1}")
        elseif(tok MATCHES "^-D(.+)$")
            list(APPEND XDC_DEFINES "${CMAKE_MATCH_1}")
        endif()
    endforeach()
else()
    message(WARNING "compiler.opt not found under ${GEN_DIR}; falling back to explicit include list")
endif()

# Explicit package include roots (used if compiler.opt parsing comes up short).
set(SDK_INCLUDES
    ${TIRTOS_INSTALL}/packages
    ${TIRTOS_INSTALL}/products/bios_6_45_01_29/packages
    ${TIRTOS_INSTALL}/products/tidrivers_tivac_2_16_00_08/packages
    ${TIRTOS_INSTALL}/products/ndk_2_25_00_09/packages
    ${TIRTOS_INSTALL}/products/ns_1_11_00_10/packages
    ${TIRTOS_INSTALL}/products/tidrivers_tivac_2_16_00_08/packages/ti/mw/wifi/cc3x00
    ${TIRTOS_INSTALL}/products/tidrivers_tivac_2_16_00_08/packages/ti/mw/wifi/cc3x00/simplelink/include
    ${TIRTOS_INSTALL}/products/tidrivers_tivac_2_16_00_08/packages/ti/mw/wifi/cc3x00/oslib
    ${TIVAWARE_INSTALL}
    ${TIVAWARE_INSTALL}/utils
)

# ---------------------------------------------------------------------------
# Generated SYS/BIOS configuration source (name carries the target suffix,
# e.g. random_pm4fg.c for the gnu M4F target). Discover it rather than hardcode.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE GEN_CFG_SRC "${GEN_DIR}/*/cfg/*.c" "${GEN_DIR}/*.c")
if(NOT GEN_CFG_SRC)
    message(FATAL_ERROR "No generated config .c found under ${GEN_DIR}")
endif()

# Generated GNU linker script.
file(GLOB_RECURSE GEN_LINKER "${GEN_DIR}/*/cfg/*.cmd" "${GEN_DIR}/linker.cmd" "${GEN_DIR}/*.cmd")
list(GET GEN_LINKER 0 LINKER_SCRIPT)
if(NOT LINKER_SCRIPT)
    message(FATAL_ERROR "No generated linker command file (*.cmd) found under ${GEN_DIR}")
endif()

# ---------------------------------------------------------------------------
# Application sources.
# ---------------------------------------------------------------------------
set(APP_SOURCES
    random.c
    EK_TM4C129EXL.c
    communication/connection_manager.c
    server/server.c
    server/inet.c
    server/udp_discovery.c
    gamepad/gamepad_input.c
    hardware/dc_motor_control.c
    hardware/stepper_control.c
    uart/uart_interface.c
)

# TivaWare driverlib + uartstdio compiled from source (prebuilt .lib is TI-format).
file(GLOB TIVAWARE_DRIVERLIB "${TIVAWARE_INSTALL}/driverlib/*.c")
set(TIVAWARE_UTILS "${TIVAWARE_INSTALL}/utils/uartstdio.c")

add_executable(${PROJECT_NAME}.out
    ${APP_SOURCES}
    ${GEN_CFG_SRC}
    ${TIVAWARE_DRIVERLIB}
    ${TIVAWARE_UTILS}
)

target_include_directories(${PROJECT_NAME}.out PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${GEN_DIR}
    ${XDC_INCLUDES}
    ${SDK_INCLUDES}
)

target_compile_definitions(${PROJECT_NAME}.out PRIVATE
    PART_TM4C129ENCPDT
    TIVAWARE
    ${XDC_DEFINES}
)

# The generated linker script pulls in the prebuilt BIOS/driver/WiFi/NDK libs.
target_link_options(${PROJECT_NAME}.out PRIVATE
    -T "${LINKER_SCRIPT}"
    -nostartfiles            # reset vectors / startup come from the BIOS config
    -Wl,-Map=${PROJECT_NAME}.map
    -Wl,--gc-sections
)
target_link_libraries(${PROJECT_NAME}.out PRIVATE c gcc m nosys)

# Rebuild if the linker script changes.
set_target_properties(${PROJECT_NAME}.out PROPERTIES LINK_DEPENDS "${LINKER_SCRIPT}")

# ---------------------------------------------------------------------------
# Post-build: raw binary + Intel hex + size report.
# ---------------------------------------------------------------------------
add_custom_command(TARGET ${PROJECT_NAME}.out POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.out> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${PROJECT_NAME}.out> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.out>
    COMMENT "Generating .bin/.hex and size report"
)
