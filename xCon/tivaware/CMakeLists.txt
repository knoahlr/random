# TivaWare component.
#
# Builds TivaWare driverlib + uartstdio as a static library from source (the
# prebuilt .lib TI ships is in TI's archive format, unusable by GNU ld).
#
# Path assumption: the vendored TivaWare tree lives at ../third_party/tivaware
# relative to this directory (i.e. xCon/third_party/tivaware). This build file
# deliberately lives OUTSIDE that tree: tools/vendor-sdk.sh deletes
# (`rm -rf xCon/third_party`) and re-copies the whole tree from the TI SDK
# install on each re-vendor. The tree is copied, not generated, so a
# hand-authored file placed under it is wiped and never restored (it isn't
# part of the SDK to copy back).
# Coupling: consumes the parent-defined `sdk_config` INTERFACE target.

get_filename_component(TIVAWARE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/tivaware ABSOLUTE)

file(GLOB TIVAWARE_DRIVERLIB ${TIVAWARE_DIR}/driverlib/*.c)
add_library(tivaware STATIC ${TIVAWARE_DRIVERLIB} ${TIVAWARE_DIR}/utils/uartstdio.c)

target_link_libraries(tivaware PUBLIC sdk_config)
target_compile_options(tivaware PRIVATE -w)
# TivaWare selects its per-compiler inline-asm (e.g. cpu.c CPUcpsie/CPUcpsid)
# with the legacy `gcc` macro, not __GNUC__. Without it cpu.c compiles empty.
target_compile_definitions(tivaware PRIVATE gcc)
