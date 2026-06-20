# Codex Project Context

This directory was reinstated after `git clean -xfd` removed the previous untracked `.codex/` contents. The original files were not tracked in Git and no recoverable copy was found in sibling workspaces, `/tmp`, or dangling Git objects.

## Firmware Layout

All firmware source and vendored dependencies live under `xCon/`:

- `xCon/app/` - application code
- `xCon/app/comms/` - TCP/UDP/UART communication code
- `xCon/app/console/` - command-line parser (`cmdline.c/h`)
- `xCon/bsp/` - board support files (`Board.h`, `EK_TM4C129EXL.c/h`)
- `xCon/generated/` - XDC/SYS/BIOS generated config artifacts
- `xCon/sysbios/` - SYS/BIOS integration and retargeting sources
- `xCon/third_party/` - vendored TI SDK/TivaWare dependencies

## Build

Use the ARM GCC CMake preset:

```sh
cmake --preset arm-gcc
cmake --build --preset arm-gcc
```

If paths in an old build cache are stale after source moves, use:

```sh
cmake --fresh --preset arm-gcc
```

## Notes

Keep project-specific Codex notes tracked or explicitly excluded before running `git clean -xfd`; otherwise Git cannot restore them.
