cmake_minimum_required(VERSION 3.20)

project(random C ASM)

# ---------------------------------------------------------------------------
# This build is self-contained: all SDK dependencies are vendored under
# xCon/third_party/ (see tools/vendor-sdk.sh). Only the one-time XDC-generated
# kernel config (xCon/generated/) comes from the bootstrap step
# (see docs/regenerating-sysbios-config.md).
# ---------------------------------------------------------------------------

set(XCON ${CMAKE_SOURCE_DIR}/xCon)
set(TP  ${XCON}/third_party)
set(SDK ${TP}/sdk)
set(WIFI ${SDK}/tidrivers/packages/ti/mw/wifi/cc3x00)
set(GEN_DIR ${XCON}/generated CACHE PATH "XDC configuro output" FORCE)

# ---------------------------------------------------------------------------
# Shared include paths + defines used consistently by the kernel and the app.
# ---------------------------------------------------------------------------
add_library(sdk_config INTERFACE)
target_include_directories(sdk_config INTERFACE
    ${XCON}
    ${XCON}/app
    ${XCON}/bsp
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
    # Build TivaWare uartstdio interrupt-driven (TX/RX ring buffers) instead of
    # polled/blocking. The UART0 ISR (UARTStdioIntHandler) is hooked to a Hwi in
    # EK_TM4C129EXL_initUART(); see docs/uart-console-retarget.md.
    UART_BUFFERED
)

# ---------------------------------------------------------------------------
# SYS/BIOS kernel, built from source (validates without the bootstrap).
# ---------------------------------------------------------------------------
include(${XCON}/sysbios/sysbios.cmake)

# ---------------------------------------------------------------------------
# TivaWare driverlib + uartstdio, built from source (prebuilt .lib is TI-format).
# ---------------------------------------------------------------------------
file(GLOB TIVAWARE_DRIVERLIB ${TP}/tivaware/driverlib/*.c)
add_library(tivaware STATIC ${TIVAWARE_DRIVERLIB} ${TP}/tivaware/utils/uartstdio.c)
target_link_libraries(tivaware PUBLIC sdk_config)
target_compile_options(tivaware PRIVATE -w)
# TivaWare selects its per-compiler inline-asm (e.g. cpu.c CPUcpsie/CPUcpsid)
# with the legacy `gcc` macro, not __GNUC__. Without it cpu.c compiles empty.
target_compile_definitions(tivaware PRIVATE gcc)

# ---------------------------------------------------------------------------
# Prebuilt GNU (.am4fg) SDK libraries: TI drivers, WiFi host driver, RTOS ports.
# ---------------------------------------------------------------------------
set(SDK_PREBUILT_LIBS
    ${SDK}/tidrivers/packages/ti/drivers/lib/drivers_tivaware.am4fg
    ${SDK}/tidrivers/packages/ti/drivers/lib/drivers_wifi_tivaware.am4fg
    ${SDK}/tidrivers/packages/ti/drivers/ports/lib/tirtosport.am4fg
    ${WIFI}/lib/cc3x00_host_driver.am4fg
    # gnu RTS: xdc.runtime bodies (System printf etc.), _c_int00 startup, syscall
    # stubs, and the TivaC catalog Boot (SysCtl clock init). From the configuro
    # linker.cmd INPUT() set; we link them here instead of via that fragment.
    ${SDK}/bios/packages/gnu/targets/arm/rtsv7M/lib/gnu.targets.arm.rtsv7M.am4fg
    ${SDK}/bios/packages/gnu/targets/arm/rtsv7M/lib/boot.am4fg
    ${SDK}/bios/packages/gnu/targets/arm/rtsv7M/lib/syscalls.am4fg
    ${SDK}/bios/packages/ti/catalog/arm/cortexm4/tiva/ce/lib/Boot.am4fg
)

# ---------------------------------------------------------------------------
# Application. Requires the one-time XDC-generated config + linker script.
# ---------------------------------------------------------------------------
set(APP_SOURCES
    ${XCON}/app/main.c
    ${XCON}/bsp/EK_TM4C129EXL.c
    ${XCON}/app/comms/connection_manager.c
    ${XCON}/app/comms/server.c
    ${XCON}/app/comms/inet.c
    ${XCON}/app/comms/udp_discovery.c
    ${XCON}/app/input/gamepad_input.c
    ${XCON}/app/hardware/dc_motor_control.c
    ${XCON}/app/hardware/stepper_control.c
    ${XCON}/app/console/cmdline.c
    ${XCON}/app/console/console.c
    ${XCON}/app/console/console_commands.c
    ${XCON}/app/comms/uart_interface.c
    ${XCON}/sysbios/newlib_locks.c   # C-library lock retargeting for gcc 13.2 newlib (see docs/newlib-locking-port.md)
    ${XCON}/sysbios/syscalls_uart.c  # newlib stdio/_sbrk retarget off semihosting onto UART0 (see docs/uart-console-retarget.md)
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
        # -lnosys (not -lrdimon): no semihosting. C-library I/O is retargeted to
        # UART0 in xCon/sysbios/syscalls_uart.c (_write/_sbrk + a no-op initialise_monitor_handles).
        -lc -lgcc -lm -lnosys
        -Wl,--end-group
    )
    # Base memory map + standard sections first, then the configuro fragment
    # (.resetVecs/xdc.meta/symbol aliases/ENTRY) -- mirrors TI-RTOS GNU makedefs.
    set(BASE_LDS ${XCON}/linker/tm4c129encpdt.lds)
    # Comma form (not "-T x") so CMake does not de-duplicate the repeated -T flag.
    target_link_options(random.out PRIVATE
        -Wl,-T,${BASE_LDS}
        -Wl,-T,${LINKER_SCRIPT}
        -nostartfiles
        -Wl,-Map=random.map
        -Wl,--gc-sections
    )
    set_target_properties(random.out PROPERTIES LINK_DEPENDS "${BASE_LDS};${LINKER_SCRIPT}")

    # -R .vtable: SYS/BIOS's RAM vector table (LMA in SRAM, zero-init, built at
    # runtime by Hwi_init). Without removing it, -O binary spans the flash->SRAM
    # gap and emits a ~512 MB file. The .out/.hex are unaffected.
    add_custom_command(TARGET random.out POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary -R .vtable $<TARGET_FILE:random.out> random.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:random.out> random.hex
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:random.out>
        COMMENT "Generating .bin/.hex and size report")
else()
    message(WARNING
        "xCon/generated/ kernel config not found -- building libraries only.\n"
        "Run the one-time bootstrap to produce random_pm4fg.c + linker.cmd "
        "(see docs/regenerating-sysbios-config.md), then reconfigure.")
endif()
