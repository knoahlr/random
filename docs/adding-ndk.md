# Adding the NDK (wired TCP/IP stack) to the build

Short guide to what it takes to bring TI's **NDK** into this CMake/GNU build.
Currently NDK is **omitted** — the firmware talks to the CC3200 over WiFi using
SimpleLink's own sockets, so the TCP/IP stack isn't needed. NDK only surfaces
because the board file (`EK_TM4C129EXL.c`) pulls in the wired-Ethernet EMAC
driver, whose header chains into NDK:

```
EK_TM4C129EXL.c -> ti/drivers/emac/EMACSnow.h -> ti/ndk/inc/stkmain.h   (not vendored)
```

So "add NDK" really means two things you can do independently:
1. **Just satisfy the headers** so the board file compiles (cheap), or
2. **Actually run the NDK stack** for wired networking (the full job).

## What NDK ships (good news)

Unlike SYS/BIOS, NDK ships **prebuilt GNU libraries** (`*.am4fg`, ~30 of them)
plus headers. So there is **no from-source kernel-style build** — it's vendor +
link, like the drivers/WiFi components. It is, however, an **RTSC package with a
config layer** (`ti/ndk/config/*` — `Global`, `Ip`, `Tcp`, `Udp`, `Emac`, ...),
which means full functional use goes through `random.cfg` + regeneration.

Locations in the SDK (`ndk_2_25_00_09`):
- Headers: `packages/ti/ndk/inc/` (74 headers; `stkmain.h`, `netmain.h`, BSD in `inc/bsd/`)
- Libs: `packages/ti/ndk/{os,stack,netctrl,nettools,tools/*}/lib/*.am4fg`
- Config (RTSC): `packages/ti/ndk/config/`

## Path 1 — just make it compile (headers only)

If you only need the board file to build (no live wired stack), vendor the NDK
**headers** and add the include path. No libs, no `.cfg` change.

1. In `tools/vendor-sdk.sh`, add an NDK section (mirror the existing `copy_ext`
   style) copying `packages/ti/ndk/inc/**` (`.h`) into
   `third_party/sdk/ndk/packages/ti/ndk/inc/`.
2. Add `third_party/sdk/ndk/packages` to the `sdk_config` include dirs in
   `CMakeLists.txt`.
3. Re-run `tools/vendor-sdk.sh`, rebuild.

This resolves `ti/ndk/inc/stkmain.h`, but any actual EMAC/NDK *calls* will fail
to link (unresolved `ti_ndk_*` / NIMU symbols). Fine if the EMAC path is dead
code; otherwise go to Path 2.

## Path 2 — actually run the NDK stack (full)

1. **Vendor headers + libs.** Extend `vendor-sdk.sh` to also copy the chosen
   `*.am4fg` libs under `third_party/sdk/ndk/.../lib/`. Pick the variant set:
   - IPv4-only, minimal: `os.am4fg`, `stk.am4fg`, `netctrl_min_ipv4.am4fg`,
     `nettool_ipv4.am4fg` (+ `servers_min_ipv4.am4fg` if you use the daemons).
   - Full / IPv6: the non-`_ipv4` and `stk6` variants.
   Don't hand-pick blindly — see step 3; configuro will tell you the exact set.

2. **Add the EMAC NIMU driver.** `EMACSnow` (in `ti/drivers/emac/`) is the NIMU
   Ethernet driver that bridges TI-drivers `EMAC` to NDK. Vendor its source/lib
   and wire an `EMAC_Config` in the board file (MAC address, ISR priority).

3. **Enable NDK in `random.cfg` and regenerate.** This is the key step — NDK is
   configured, not just linked. Add (roughly):
   ```js
   var Global = xdc.useModule('ti.ndk.config.Global');
   var Ip     = xdc.useModule('ti.ndk.config.Ip');
   var Tcp    = xdc.useModule('ti.ndk.config.Tcp');
   var Udp    = xdc.useModule('ti.ndk.config.Udp');
   // stack thread, buffers, services (DHCP client, etc.)
   ```
   Then re-run `tools/generate-bios-config.sh` (XDCtools — same one-time flow as
   the kernel; see `docs/regenerating-sysbios-config.md`). configuro will:
   - regenerate `random_pm4fg.c` with the NDK module state, and
   - **emit the correct NDK `*.am4fg` set into `generated/linker.cmd`'s
     `INPUT()`** — that list is your authoritative lib selection for step 1.
   Add `XDCPATH` already includes NDK (`generate-bios-config.sh` sets `NDK=...`),
   so only the `.cfg` needs changing.

4. **Link in CMake.** Add the NDK libs to the app link group (alongside the
   existing `.am4fg` set), inside the `--start-group/--end-group`.

## Gotchas

- **Regeneration required.** Functional NDK changes the generated kernel config;
  you cannot add it by CMake alone. Budget one XDCtools run.
- **Prebuilt ABI vintage.** Like the other `.am4fg` libs, NDK was built with the
  old GCC ARM (4.8.x). Same watch-item as the rest (see
  `docs/tivac-cmake-migration.md` §4) — Cortex-M4F AAPCS is stable but
  newlib/libgcc edges can bite.
- **Two socket stacks don't mix.** SimpleLink already provides a BSD-style socket
  API (`sl_Socket`, and its `select`/`fd_set`/`timeval`). NDK provides *its own*
  BSD sockets. Running both means resolving symbol/namespace collisions
  (the same family as the current SimpleLink-vs-newlib `select` clash). Decide
  which stack owns sockets; don't expose both BSD namespaces in one TU.
- **NDK `os` layer ties to SYS/BIOS.** `os.am4fg` is the NDK→BIOS adaptation
  (tasks, semaphores, clock). It must match the BIOS you're building; it does
  here (same SDK bundle).

## Recommendation

For this firmware (WiFi via CC3200), prefer **Path 1** or simply excluding the
EMAC/Ethernet path from the board file — NDK's wired stack is not needed. Only do
Path 2 if you actually want wired Ethernet networking on the TM4C129's MAC.
