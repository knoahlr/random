# Toolchain file for the GNU Arm Embedded toolchain (arm-none-eabi-gcc),
# targeting the TM4C129ENCPDT (Cortex-M4F, single-precision FPU).
#
# Usage:
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake ...
# or via the "arm-gcc" preset in CMakePresets.json.

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Allow overriding the toolchain prefix / location from the cache.
set(TOOLCHAIN_PREFIX "arm-none-eabi-" CACHE STRING "Cross-compiler prefix")
set(TOOLCHAIN_PATH   ""               CACHE PATH   "Optional dir containing the toolchain (leave empty if on PATH)")

if(TOOLCHAIN_PATH)
    set(_tc "${TOOLCHAIN_PATH}/${TOOLCHAIN_PREFIX}")
else()
    set(_tc "${TOOLCHAIN_PREFIX}")
endif()

set(CMAKE_C_COMPILER   "${_tc}gcc")
set(CMAKE_ASM_COMPILER "${_tc}gcc")
set(CMAKE_CXX_COMPILER "${_tc}g++")
set(CMAKE_OBJCOPY      "${_tc}objcopy" CACHE FILEPATH "")
set(CMAKE_OBJDUMP      "${_tc}objdump" CACHE FILEPATH "")
set(CMAKE_SIZE         "${_tc}size"    CACHE FILEPATH "")

# We are building for bare metal: skip the link step in compiler detection.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Cortex-M4F core flags. Must match the XDC target (gnu.targets.arm.M4F) the
# BIOS config + prebuilt libraries were generated/built for.
set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")

set(CMAKE_C_FLAGS_INIT   "${CPU_FLAGS} -ffunction-sections -fdata-sections")
set(CMAKE_ASM_FLAGS_INIT "${CPU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CPU_FLAGS} -Wl,--gc-sections")

# Only look for programs on the host; libs/headers come from the SDK paths.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
