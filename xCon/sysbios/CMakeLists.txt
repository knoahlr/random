# SYS/BIOS kernel component.
#
# Builds SYS/BIOS as a static library from the exact source set that XDC
# `configuro` selected for xCon/sysbios/random.cfg (the 29 files listed in the
# generated sysbios makefile). See docs/regenerating-sysbios-config.md. If random.cfg
# changes the set of BIOS modules, re-sync this list + SYSBIOS_FEATURE_DEFS
# from xCon/generated/package/cfg/random_pm4fg.src/sysbios/makefile.
#
# Path assumption: the vendored SDK lives at ../third_party/sdk relative to this
# directory (i.e. xCon/third_party/sdk). The kernel sources are rooted there.
# Coupling: consumes the parent-defined `sdk_config` INTERFACE target for the
# shared include paths + xdc/tivaware defines.

get_filename_component(SYSBIOS_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/sdk/bios/packages/ti/sysbios ABSOLUTE)

set(SYSBIOS_C_SOURCES
    BIOS.c
    knl/Clock.c knl/Idle.c knl/Intrinsics.c knl/Event.c knl/Mailbox.c
    knl/Queue.c knl/Semaphore.c knl/Swi.c knl/Swi_andn.c knl/Task.c
    hal/Hwi.c hal/Hwi_stack.c hal/Hwi_startup.c hal/Seconds.c hal/SecondsClock.c
    family/arm/m3/Hwi.c family/arm/m3/TaskSupport.c
    rts/gnu/ReentSupport.c rts/gnu/SemiHostSupport.c
    gates/GateHwi.c gates/GateMutex.c
    heaps/HeapMem.c
    family/arm/lm4/Seconds.c family/arm/lm4/Timer.c
)

# GNU assembler-with-cpp sources (Cortex-M3/M4 Hwi/Task context switch, intrinsics).
set(SYSBIOS_ASM_SOURCES
    family/arm/m3/Hwi_asm_gnu.sv7M
    family/arm/m3/Hwi_asm_switch_gnu.sv7M
    family/arm/m3/IntrinsicsSupport_asm_gnu.sv7M
    family/arm/m3/TaskSupport_asm_gnu.sv7M
)

list(TRANSFORM SYSBIOS_C_SOURCES   PREPEND ${SYSBIOS_DIR}/)
list(TRANSFORM SYSBIOS_ASM_SOURCES PREPEND ${SYSBIOS_DIR}/)

# Compile-time feature configuration (BIOS_DEFS from the generated makefile).
set(SYSBIOS_FEATURE_DEFS
    ti_sysbios_BIOS_swiEnabled__D=TRUE
    ti_sysbios_BIOS_taskEnabled__D=TRUE
    ti_sysbios_BIOS_clockEnabled__D=TRUE
    ti_sysbios_BIOS_runtimeCreatesEnabled__D=TRUE
    ti_sysbios_hal_Hwi_DISABLE_ALL_HOOKS
    ti_sysbios_knl_Swi_DISABLE_ALL_HOOKS
    ti_sysbios_BIOS_smpEnabled__D=FALSE
    ti_sysbios_Build_useHwiMacros
    ti_sysbios_knl_Swi_numPriorities__D=16
    ti_sysbios_knl_Task_deleteTerminatedTasks__D=FALSE
    ti_sysbios_knl_Task_numPriorities__D=16
    ti_sysbios_knl_Task_checkStackFlag__D=FALSE
    ti_sysbios_knl_Task_initStackFlag__D=TRUE
    ti_sysbios_knl_Clock_TICK_SOURCE=ti_sysbios_knl_Clock_TickSource_TIMER
    ti_sysbios_knl_Clock_TICK_MODE=ti_sysbios_knl_Clock_TickMode_PERIODIC
    ti_sysbios_hal_Core_delegate_getId=ti_sysbios_hal_CoreNull_getId__E
    ti_sysbios_hal_Core_delegate_interruptCore=ti_sysbios_hal_CoreNull_interruptCore__E
    ti_sysbios_hal_Core_delegate_lock=ti_sysbios_hal_CoreNull_lock__E
    ti_sysbios_hal_Core_delegate_unlock=ti_sysbios_hal_CoreNull_unlock__E
    ti_sysbios_hal_Core_numCores__D=1
    ti_sysbios_hal_CoreNull_numCores__D=1
    ti_sysbios_utils_Load_taskEnabled__D=TRUE
    ti_sysbios_utils_Load_swiEnabled__D=FALSE
    ti_sysbios_utils_Load_hwiEnabled__D=FALSE
    ti_sysbios_family_arm_m3_Hwi_dispatcherSwiSupport__D=TRUE
    ti_sysbios_family_arm_m3_Hwi_dispatcherTaskSupport__D=TRUE
    ti_sysbios_family_arm_m3_Hwi_dispatcherAutoNestingSupport__D=TRUE
    ti_sysbios_family_arm_m3_Hwi_dispatcherIrpTrackingSupport__D=TRUE
    ti_sysbios_knl_Semaphore_supportsEvents__D=FALSE
    ti_sysbios_knl_Semaphore_supportsPriority__D=FALSE
    xdc_runtime_Assert_DISABLE_ALL
    xdc_runtime_Log_DISABLE_ALL
)

add_library(sysbios STATIC ${SYSBIOS_C_SOURCES} ${SYSBIOS_ASM_SOURCES})

set_source_files_properties(${SYSBIOS_ASM_SOURCES} PROPERTIES
    LANGUAGE C
    COMPILE_OPTIONS "-x;assembler-with-cpp")

target_link_libraries(sysbios PUBLIC sdk_config)
target_compile_definitions(sysbios PRIVATE ${SYSBIOS_FEATURE_DEFS})
# Vendored kernel sources target gcc 4.8-era; silence warnings and relax aliasing.
target_compile_options(sysbios PRIVATE -w -fno-strict-aliasing)
