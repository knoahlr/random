cmake_minimum_required(VERSION 3.20)

project(random C ASM)

# ---------------------------------------------------------------------------
# This build is self-contained: all SDK dependencies are vendored under
# third_party/ (see tools/vendor-sdk.sh). Only the one-time XDC-generated
# kernel config (generated/) comes from the bootstrap step
# (see docs/regenerating-sysbios-config.md).
# ---------------------------------------------------------------------------

set(TP  ${CMAKE_SOURCE_DIR}/third_party)
set(SDK ${TP}/sdk)
set(WIFI ${SDK}/tidrivers/packages/ti/mw/wifi/cc3x00)
set(GEN_DIR ${CMAKE_SOURCE_DIR}/generated CACHE PATH "XDC configuro output")

# ---------------------------------------------------------------------------
# Shared include paths + defines used consistently by the kernel and the app.
# ---------------------------------------------------------------------------
add_library(sdk_config INTERFACE)
target_include_directories(sdk_config INTERFACE
    ${CMAKE_SOURCE_DIR}
    ${GEN_DIR}
    ${SDK}/bios/packages
    ${SDK}/xdc/packages
    ${SDK}/tidrivers/packages
    ${WIFI}
    ${WIFI}/simplelink/include
    ${WIFI}/oslib
    ${TP}/tivaware
)
target_compile_definitions(sdk_config INTERFACE
    xdc_target_types__=gnu/targets/arm/std.h
    xdc_target_name__=M4F
    PART_TM4C129ENCPDT
    TIVAWARE
)

# ---------------------------------------------------------------------------
# SYS/BIOS kernel, built from source (validates without the bootstrap).
# ---------------------------------------------------------------------------
include(cmake/sysbios.cmake)

# ---------------------------------------------------------------------------
# TivaWare driverlib + uartstdio, built from source (prebuilt .lib is TI-format).
# ---------------------------------------------------------------------------
file(GLOB TIVAWARE_DRIVERLIB ${TP}/tivaware/driverlib/*.c)
add_library(tivaware STATIC ${TIVAWARE_DRIVERLIB} ${TP}/tivaware/utils/uartstdio.c)
target_link_libraries(tivaware PUBLIC sdk_config)
target_compile_options(tivaware PRIVATE -w)

# ---------------------------------------------------------------------------
# Prebuilt GNU (.am4fg) SDK libraries: TI drivers, WiFi host driver, RTOS ports.
# ---------------------------------------------------------------------------
set(SDK_PREBUILT_LIBS
    ${SDK}/tidrivers/packages/ti/drivers/lib/drivers_tivaware.am4fg
    ${SDK}/tidrivers/packages/ti/drivers/lib/drivers_wifi_tivaware.am4fg
    ${SDK}/tidrivers/packages/ti/drivers/ports/lib/tirtosport.am4fg
    ${WIFI}/lib/cc3x00_host_driver.am4fg
)

# ---------------------------------------------------------------------------
# Application. Requires the one-time XDC-generated config + linker script.
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

file(GLOB GEN_CFG_SRC ${GEN_DIR}/package/cfg/random_pm4fg.c ${GEN_DIR}/*.c)
file(GLOB GEN_LINKER  ${GEN_DIR}/linker.cmd ${GEN_DIR}/package/cfg/*.cmd ${GEN_DIR}/*.cmd)

if(GEN_CFG_SRC AND GEN_LINKER)
    list(GET GEN_LINKER 0 LINKER_SCRIPT)
    add_executable(random.out ${APP_SOURCES} ${GEN_CFG_SRC})
    target_link_libraries(random.out PRIVATE sdk_config)
    target_link_libraries(random.out PRIVATE
        -Wl,--start-group
        sysbios tivaware ${SDK_PREBUILT_LIBS}
        -lc -lgcc -lm -lnosys
        -Wl,--end-group
    )
    target_link_options(random.out PRIVATE
        -T ${LINKER_SCRIPT}
        -nostartfiles
        -Wl,-Map=random.map
        -Wl,--gc-sections
    )
    set_target_properties(random.out PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT})

    add_custom_command(TARGET random.out POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:random.out> random.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:random.out> random.hex
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:random.out>
        COMMENT "Generating .bin/.hex and size report")
else()
    message(WARNING
        "generated/ kernel config not found -- building libraries only.\n"
        "Run the one-time bootstrap to produce random_pm4fg.c + linker.cmd "
        "(see docs/regenerating-sysbios-config.md), then reconfigure.")
endif()
