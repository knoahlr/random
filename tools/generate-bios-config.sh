#!/usr/bin/env bash
#
# One-time (and only) invocation of XDCtools `configuro` to generate the
# SYS/BIOS static configuration from random.cfg for the GNU Arm target.
#
# After this runs and you commit the contents of generated/, XDCtools is no
# longer needed to build the project -- CMake compiles the generated C as
# ordinary source and links with the generated linker command file.
#
# Re-run this ONLY when you intentionally change kernel configuration in
# random.cfg (tick period, heap size, statically-defined Hwis, etc.).
#
# NOTE on paths: xs.exe is a Windows binary, so it emits Windows paths
# (C:/ti/...) into compiler.opt and linker.cmd. We sed-convert them to WSL
# (/mnt/c/ti/...) at the end so the GNU/WSL build can consume them.

set -euo pipefail

# ---- Adjust these to your install locations -------------------------------
TI_ROOT="/mnt/c/ti"
XDCTOOLS="${TI_ROOT}/xdctools_3_32_00_06_core"
TIRTOS="${TI_ROOT}/tirtos_tivac_2_16_00_08"
BIOS="${TIRTOS}/products/bios_6_45_01_29"
TIDRIVERS="${TIRTOS}/products/tidrivers_tivac_2_16_00_08"
NDK="${TIRTOS}/products/ndk_2_25_00_09"
NS="${TIRTOS}/products/ns_1_11_00_10"
UIA="${TIRTOS}/products/uia_2_00_05_50"

# Root of the GNU Arm toolchain install (the dir that contains bin/arm-none-eabi-gcc).
# configuro needs this to compile/validate the generated config.
GNU_TOOLS="${GNU_TOOLS:-/usr/lib/arm-none-eabi}"   # override: GNU_TOOLS=/path ./generate-bios-config.sh

TARGET="gnu.targets.arm.M4F"
PLATFORM="ti.platforms.tiva:TM4C129ENCPDT"
PROJ_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${PROJ_DIR}/generated"
CFG="${PROJ_DIR}/random.cfg"
# ---------------------------------------------------------------------------

# xs.exe wants Windows-style paths for its arguments; convert via wslpath.
to_win() { wslpath -w "$1"; }

XDCPATH="$(to_win "${BIOS}/packages");$(to_win "${TIDRIVERS}/packages");$(to_win "${NDK}/packages");$(to_win "${NS}/packages");$(to_win "${UIA}/packages");$(to_win "${XDCTOOLS}/packages")"

echo ">> Running configuro (target=${TARGET}, platform=${PLATFORM})"
rm -rf "${OUT_DIR}"
mkdir -p "${OUT_DIR}"

"${XDCTOOLS}/xs.exe" --xdcpath="${XDCPATH}" xdc.tools.configuro \
    -c "$(to_win "${GNU_TOOLS}")" \
    -t "${TARGET}" \
    -p "${PLATFORM}" \
    -r release \
    -o "$(to_win "${OUT_DIR}")" \
    "$(to_win "${CFG}")"

echo ">> Converting emitted Windows paths (C:/ti -> /mnt/c/ti) for WSL builds"
# configuro writes compiler.opt and a linker command file under OUT_DIR.
find "${OUT_DIR}" -type f \( -name 'compiler.opt' -o -name '*.cmd' -o -name 'linker.cmd' -o -name '*.mak' \) -print0 \
  | xargs -0 -r sed -i -E 's#([A-Za-z]):[\\/]#/mnt/\L\1\E/#g; s#\\#/#g'

echo ">> Done. Generated config is in: ${OUT_DIR}"
echo "   Commit it, then build with CMake. XDCtools is no longer needed."
