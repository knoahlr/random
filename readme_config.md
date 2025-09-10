# random.cfg and Memory Map Analysis

This file summarizes the configuration found in `random.cfg` and details the memory mapping and RTOS setup for the TM4C129ENCPDT-based firmware in this repository.

---

## Purpose of `random.cfg`

This file configures TI-RTOS (SYS/BIOS) kernel, memory, and system for the EK-TM4C129EXL development board. It specifies placement for code/data, RTOS kernel options, debug/exception settings, tasks, and TI driver library options.

---

## Memory Map and Section Placement

### Stack and Heap

- **System/ISR/Swi stack:**
  - `Program.stack = 0x2800` (10240 bytes, 10 KB)
  - Used by all ISRs and the main context.
- **Heap:**
  - `Program.heap = 0x800` (2048 bytes, 2 KB)
  - Used for dynamic allocation (e.g., tasks/RTOS objects at runtime).
- **Each RTOS Task:**
  - Default stack size: 512 bytes
- **Idle Task Stack:**
  - 512 bytes (if enabled)

### Section Mapping

| Section           | Location         | Contents                               |
|-------------------|------------------|----------------------------------------|
| .resetVecs        | FLASH @ 0x0      | Reset/vector table (entry/IRQ vectors) |
| .vecs             | SRAM @ 0x2000_0000| Optional RAM-copy of vectors            |
| .text/.const      | FLASH            | Code and read-only data                |
| .cinit/.pinit, .init_array| FLASH    | Initialization routines, C++ support   |
| .ARM.exidx/.ARM.extab | FLASH        | ARM unwind tables (if present)         |
| .bss/.data        | SRAM             | Global/static variables                |
| .stack            | SRAM             | Main/system stack                      |
| .sysmem/.heap     | SRAM             | Dynamic memory allocation              |
| .TI.noinit/.TI.persistent | SRAM     | Special TI sections (no-init/persist)  |
| __llvm_prf_*      | SRAM/FLASH       | LLVM profiling data (if present)       |

---

## RTOS Runtime/Performance Setup

- **RTOS**: TI-RTOS / SYS/BIOS
- **Tick period**: 1000 us (1 ms system tick)
- **Asserts/Logs**: Disabled (`assertsEnabled = false`, `logsEnabled = false`) for smaller code size
- **Named modules/text**: Module names retained (`namedModule = true`, `Text.isLoaded = true`) for readable errors
- **Error Handling**: Decoding/printing enabled (`Error.policyDefault`, print hook)
- **Task configuration:**
  - Default stack: 512 bytes per task
  - Idle task disabled (`enableIdleTask = false`)
  - Stack checks disabled for both tasks and HWIs
- **Driver Libraries**: Non-instrumented versions for minimal code size
- **Semaphore and Events**: Priority-based queueing/events mostly disabled to save memory
- **BIOS.libType**: Set to Debug library (easy debugging, not optimized)

---

## Exception & Debug Settings

- Hardware exceptions for divide-by-zero and unaligned access are disabled (minimal flash, riskier runtime)
- Full ARM exception decode/print enabled
- System abort/exit handlers are standard

---

## Summary Table

| Section            | Location         | Size        | Contents                        |
|--------------------|------------------|-------------|---------------------------------|
| .resetVecs         | Flash 0x0        | -           | Reset vector table              |
| .vecs              | SRAM 0x20000000  | -           | RAM vector table (if used)      |
| .text, .const      | FLASH            | -           | Program code, constants         |
| .bss, .data        | SRAM             | -           | Init/uninit global variables    |
| .stack             | SRAM             | 10 KB       | Main/system stack               |
| .sysmem/.heap      | SRAM             | 2 KB        | Heap/malloc                     |
| Per-task stack     | SRAM             | 512 B each  | RTOS task stack (default)       |

---

## Additional Notes

- Program/BIOS heap: There is both `Program.heap` (2 KB for linker) and `BIOS.heapSize` (32 KB for RTOS), which might be a potential configuration oversight—ensure only one is active, or that they match if dynamic allocation is needed.
- All peripheral and RTOS objects are expected to use this memory map for placement.
- Vector table placement allows for either ROM- or RAM-based interrupt vectoring.

---

This summary provides a snapshot of the firmware’s system and memory configuration based on `random.cfg`. For actual memory usage at build time, consult the generated linker map file after a build (not included here).
