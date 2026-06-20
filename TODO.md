# TODO

Open items after the CCS → CMake migration. **None of these block the build** —
`random.out`/`.bin`/`.hex` link from a clean tree today. See
[docs/tivac-cmake-migration.md](docs/tivac-cmake-migration.md) for context and
[docs/quirks.md](docs/quirks.md) for the gotchas reference.

## Cleanup

- [ ] **Remove legacy CCS files** now that the CMake build is complete:
  `.cproject`, `.ccsproject`, `.project`, `makefile.defs`, `.config/`,
  `targetConfigs/`.
  **Keep `src/`** — it now holds `newlib_locks.c` (the C-library lock port).
- [ ] **Trim `CMakePresets.json`** — drop the now-unused `TI_ROOT`,
  `TIRTOS_INSTALL`, `TIVAWARE_INSTALL` variables (the build reads everything from
  `third_party/`).

## Runtime / correctness

- [x] **Retarget C-library I/O off semihosting.** Done: link `-lnosys` (was
  `-lrdimon`) and `src/syscalls_uart.c` retargets `_write`/`_isatty`/`_sbrk` +
  no-op `initialise_monitor_handles` onto **UART0** (the ICDI virtual COM port).
  `printf`/`puts` now work standalone, no debugger. See
  [docs/uart-console-retarget.md](docs/uart-console-retarget.md). Optional
  follow-up: drop the now-inert `SemiHostSupport` from `random.cfg` (needs a
  config regeneration).

## Watch-items (verify, no action unless they bite)

- [ ] **Prebuilt-library ABI vintage.** The vendored `.am4fg` archives (drivers,
  CC3x00 WiFi, gnu RTS, catalog Boot) were built with **GCC ARM 4.8.4**; we build
  with **13.2**. Cortex-M4F AAPCS is stable, but newlib/libgcc edges can differ.
  The CC3x00 WiFi driver is prebuilt-only (no source), so this can't be fully
  escaped — watch link/runtime behaviour, especially around the C library.
