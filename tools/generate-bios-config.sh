#!/usr/bin/env bash
#
# One-time (and only) invocation of XDCtools `configuro` to generate the
# SYS/BIOS static configuration from random.cfg for the GNU Arm target.
#
# After this runs and you commit generated/random_pm4fg.c + linker.cmd,
# XDCtools is no longer needed to build the project -- CMake compiles the
# generated C as ordinary source and links the from-source kernel.
#
# Re-run this ONLY when you intentionally change kernel configuration in
# random.cfg (tick period, heap size, statically-defined Hwis, etc.).
#
# Requires the *Linux* XDCtools (same version as the SDK) so it can drive the
# native arm-none-eabi-gcc. The Windows XDCtools cannot exec a Linux compiler.

set -euo pipefail

# ---- Adjust these to your install locations -------------------------------
XDCTOOLS="${XDCTOOLS:-/opt/ti/xdctools_3_32_00_06_core}"
TIRTOS="${TIRTOS:-/mnt/c/ti/tirtos_tivac_2_16_00_08}"
BIOS="${TIRTOS}/products/bios_6_45_01_29"
TIDRIVERS="${TIRTOS}/products/tidrivers_tivac_2_16_00_08"
NDK="${TIRTOS}/products/ndk_2_25_00_09"
NS="${TIRTOS}/products/ns_1_11_00_10"
UIA="${TIRTOS}/products/uia_2_00_05_50"

# Root of the GNU Arm toolchain (dir containing bin/arm-none-eabi-gcc).
GNU_TOOLS="${GNU_TOOLS:-/usr}"

TARGET="gnu.targets.arm.M4F"
PLATFORM="ti.platforms.tiva:TM4C129ENCPDT"
PROJ_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${PROJ_DIR}/generated"
CFG="${PROJ_DIR}/random.cfg"
# ---------------------------------------------------------------------------

# configuro builds the custom SYS/BIOS library from SDK source; the gnu/newlib
# ReentSupport canary breaks under modern gcc. Neutralize it in the SDK source
# (idempotent) so the internal build succeeds -- mirrors our vendored patch.
REENT="${BIOS}/packages/ti/sysbios/rts/gnu/ReentSupport.c"
if [ -f "${REENT}" ] && grep -q 'lock.init_done = 1;' "${REENT}"; then
    echo ">> Patching SDK ReentSupport.c canary for modern newlib (idempotent)"
    sed -i 's/^\( *\)lock.init_done = 1;/\1\/* patched: opaque _LOCK_T *\/ (void)0;/' "${REENT}"
fi

XDCPATH="${BIOS}/packages;${TIDRIVERS}/packages;${NDK}/packages;${NS}/packages;${UIA}/packages;${XDCTOOLS}/packages"

echo ">> Running configuro (target=${TARGET}, platform=${PLATFORM})"
rm -rf "${OUT_DIR}"
mkdir -p "${OUT_DIR}"

"${XDCTOOLS}/xs" --xdcpath="${XDCPATH}" xdc.tools.configuro \
    -c "${GNU_TOOLS}" \
    -t "${TARGET}" \
    -p "${PLATFORM}" \
    -r release \
    -o "${OUT_DIR}" \
    "${CFG}"

# The gnu RTS template emits obsolete __libc_lock_* glue (pre-newlib-3.0, struct
# _LOCK_T) that does not compile against modern newlib. Disable that block; the
# reent-model hooks newlib actually calls live in src/newlib_locks.c. Idempotent.
# See docs/newlib-locking-port.md.
GEN_CFG="${OUT_DIR}/package/cfg/random_pm4fg.c"
if [ -f "${GEN_CFG}" ] && ! grep -q 'LOCAL PATCH: obsolete newlib __libc_lock' "${GEN_CFG}"; then
    echo ">> Disabling obsolete __libc_lock_* block in generated config (idempotent)"
    perl -0777 -i -pe 's{(\nvoid __libc_lock_init\(_LOCK_T \*lock\)\n.*?\n\}\n)(\n/\*\n \* ======== ti\.sysbios\.rts\.gnu\.SemiHostSupport TEMPLATE)}{\n#if 0 /* LOCAL PATCH: obsolete newlib __libc_lock_* glue; see src/newlib_locks.c */$1#endif /* LOCAL PATCH */$2}s' "${GEN_CFG}"
fi

# The generated linker.cmd is used only for its SECTIONS/symbols (layered on top
# of linker/tm4c129encpdt.lds). Its INPUT(...) block pulls the configuro-built
# random_pm4fg.om4fg + sysbios.am4fg (which duplicate our from-source objects)
# and references libs by absolute /mnt/c/ti paths. CMake provides all objects and
# libraries from third_party/, so strip the INPUT() block. Idempotent.
GEN_LNK="${OUT_DIR}/linker.cmd"
if [ -f "${GEN_LNK}" ] && grep -q '^INPUT(' "${GEN_LNK}"; then
    echo ">> Stripping INPUT() block from generated linker.cmd (idempotent)"
    perl -0777 -i -pe 's{\nINPUT\(\n.*?\n\)\n}{\n/* LOCAL PATCH (post-generation): configuro INPUT() block removed; CMake\n * provides all objects/libraries from third_party/. See docs/. */\n}s' "${GEN_LNK}"
fi

echo ">> Done. Generated config is in: ${OUT_DIR}"
echo "   Key outputs: random_pm4fg.c (kernel config) and the linker command file."
echo "   Commit those; XDCtools is no longer needed to build."
