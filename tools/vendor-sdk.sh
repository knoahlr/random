#!/usr/bin/env bash
#
# Vendor the exact SDK subset this firmware needs into third_party/ so the repo
# builds self-contained (cmake + ninja + arm-none-eabi-gcc) with no TI install.
#
# What gets vendored (pruned to headers + GNU libs + the BIOS sources we compile):
#   third_party/tivaware/            TivaWare driverlib + utils + inc (built from source)
#   third_party/sdk/bios/packages/   SYS/BIOS source+headers + gnu target headers
#   third_party/sdk/xdc/packages/    xdc.runtime / xdc.std headers
#   third_party/sdk/tidrivers/packages/  ti.drivers + cc3x00 WiFi headers + GNU .am4fg libs
#
# NDK is intentionally omitted (the app uses SimpleLink's own sockets). Add it
# only if the final link reports unresolved ti_ndk_* symbols.
#
# This is a one-time bootstrap (re-run only to refresh against a newer SDK).

set -euo pipefail

# ---- Source SDK locations -------------------------------------------------
TI_ROOT="${TI_ROOT:-/mnt/c/ti}"
TIVAWARE="${TI_ROOT}/TivaWare_C_Series-2.2.0.295"
TIRTOS="${TI_ROOT}/tirtos_tivac_2_16_00_08"
BIOS="${TIRTOS}/products/bios_6_45_01_29/packages"
TIDRIVERS="${TIRTOS}/products/tidrivers_tivac_2_16_00_08/packages"
XDCTOOLS="${TI_ROOT}/xdctools_3_32_00_06_core/packages"
# ---------------------------------------------------------------------------

PROJ_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TP="${PROJ_DIR}/third_party"
WIFI="ti/mw/wifi/cc3x00"

echo ">> Vendoring SDK subset into ${TP}"
rm -rf "${TP}"
mkdir -p "${TP}/tivaware" \
         "${TP}/sdk/bios/packages" \
         "${TP}/sdk/xdc/packages" \
         "${TP}/sdk/tidrivers/packages"

# rsync filter helpers: copy a tree keeping only matching extensions, pruning empty dirs.
copy_ext() {  # copy_ext SRC DST "ext1 ext2 ..."
    local src="$1" dst="$2"; shift 2
    local includes=()
    for ext in "$@"; do includes+=(--include="*.${ext}"); done
    mkdir -p "${dst}"
    rsync -am --include='*/' "${includes[@]}" --exclude='*' "${src}/" "${dst}/"
}

# --- TivaWare (compiled from source) ---------------------------------------
echo ">> TivaWare driverlib + utils + inc"
copy_ext "${TIVAWARE}/driverlib" "${TP}/tivaware/driverlib" c h
copy_ext "${TIVAWARE}/inc"       "${TP}/tivaware/inc"       h
mkdir -p "${TP}/tivaware/utils"
cp "${TIVAWARE}/utils/uartstdio.c" "${TIVAWARE}/utils/uartstdio.h" "${TP}/tivaware/utils/"

# --- SYS/BIOS (compiled from source) + gnu target headers ------------------
echo ">> SYS/BIOS source + headers (this is the largest tree)"
copy_ext "${BIOS}/ti/sysbios" "${TP}/sdk/bios/packages/ti/sysbios" h c sv7M asm s
copy_ext "${BIOS}/ti/catalog" "${TP}/sdk/bios/packages/ti/catalog" h
copy_ext "${BIOS}/gnu"        "${TP}/sdk/bios/packages/gnu"        h
# xdc runtime/std headers come from the xdctools packages
echo ">> xdc.runtime / xdc.std headers"
copy_ext "${XDCTOOLS}/xdc" "${TP}/sdk/xdc/packages/xdc" h

# --- TI drivers + WiFi (prebuilt GNU libs + headers) -----------------------
echo ">> ti.drivers + CC3x00 WiFi headers"
copy_ext "${TIDRIVERS}/ti/drivers" "${TP}/sdk/tidrivers/packages/ti/drivers" h
copy_ext "${TIDRIVERS}/${WIFI}"    "${TP}/sdk/tidrivers/packages/${WIFI}"    h
echo ">> prebuilt GNU (.am4fg) libraries"
copy_ext "${TIDRIVERS}/ti/drivers/lib"       "${TP}/sdk/tidrivers/packages/ti/drivers/lib"       am4fg
copy_ext "${TIDRIVERS}/ti/drivers/ports/lib" "${TP}/sdk/tidrivers/packages/ti/drivers/ports/lib" am4fg
copy_ext "${TIDRIVERS}/${WIFI}/lib"          "${TP}/sdk/tidrivers/packages/${WIFI}/lib"          am4fg

echo ">> Done."
du -sh "${TP}"/* 2>/dev/null || true
echo ">> Vendored. CMake reads from third_party/ (see CMakeLists.txt)."
