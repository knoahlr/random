# Vendoring TI Driver Sources

This project normally links prebuilt TI-RTOS/TI-Drivers archives from
`xCon/third_party/sdk`. Some vendor objects are acceptable as black boxes, but
source should be vendored when we need to inspect or change driver behavior that
affects hardware safety or fault isolation.

## Current Vendored Driver

`xCon/third_party/sdk/tidrivers/packages/ti/drivers/spi/SPITivaDMA.c` is
vendored from the local TI-RTOS install:

```text
/mnt/c/ti/tirtos_tivac_2_16_00_08/products/tidrivers_tivac_2_16_00_08/packages/ti/drivers/spi/SPITivaDMA.c
```

The repo still uses the public TI header:

```text
xCon/third_party/sdk/tidrivers/packages/ti/drivers/spi/SPITivaDMA.h
```

The source is compiled directly by `CMakeLists.txt`, so its symbols satisfy the
SPI function table instead of pulling the same implementation from a prebuilt
archive.

## Why SPITivaDMA.c Is Vendored

The CC3100 SimpleLink path can call `spi_Write(fd, pBuff, len)` with `pBuff`
pointing into TM4C flash/rodata. The stock `SPITivaDMA` implementation passes
`SPI_Transaction.txBuf`/`rxBuf` through to `uDMAChannelTransferSet()`, which
programs the uDMA control table.

For this board, DMA transfer buffers must be constrained to TM4C SRAM. The
vendored driver enforces that immediately before uDMA descriptors are
programmed. If a TX or RX transaction buffer is outside SRAM, the driver uses
per-SSI SRAM bounce buffers so uDMA never sees a flash/rodata buffer address.

## Procedure

1. Locate the source in the TI-RTOS install.

```sh
rg --files /mnt/c/ti/tirtos_tivac_2_16_00_08/products/tidrivers_tivac_2_16_00_08/packages \
  | rg '/SPITivaDMA\.c$'
```

2. Copy the vendor source into the matching repo package path.

```sh
cp /mnt/c/ti/tirtos_tivac_2_16_00_08/products/tidrivers_tivac_2_16_00_08/packages/ti/drivers/spi/SPITivaDMA.c \
  xCon/third_party/sdk/tidrivers/packages/ti/drivers/spi/SPITivaDMA.c
```

3. Add the source file to `APP_SOURCES` in `CMakeLists.txt`.

```cmake
${XCON}/third_party/sdk/tidrivers/packages/ti/drivers/spi/SPITivaDMA.c
```

4. Keep the original license header intact. Make only the smallest local changes
   needed, and leave comments explaining why the behavior differs from stock TI.

5. Build and inspect the linked symbols.

```sh
cmake --build --preset arm-gcc
arm-none-eabi-nm -an build/random.out | rg 'SPITivaDMA_transfer|SPI_transfer|__wrap_SPI_transfer'
grep -n 'SPITivaDMA.c' build/random.map
```

Expected:

```text
SPITivaDMA_transfer
SPI_transfer
```

There should not be a `__wrap_SPI_transfer` symbol for this fix. The map should
show `SPITivaDMA.c.obj` coming from the repo build, not only from a TI archive.

## SimpleLink Source

The repo currently carries mostly CC3100 SimpleLink headers plus prebuilt
archives. The local TI-RTOS install also has SimpleLink source under:

```text
/mnt/c/ti/tirtos_tivac_2_16_00_08/products/tidrivers_tivac_2_16_00_08/packages/ti/mw/wifi/cc3x00/simplelink/source/
```

Important files there include:

```text
driver.c
device.c
wlan.c
socket.c
netcfg.c
netapp.c
fs.c
flowcont.c
spawn.c
nonos.c
```

If SimpleLink command-buffer ownership needs to be fixed at the source of
`pBuff`, vendor those files as a separate, deliberate change and verify the
prebuilt CC3100 host-driver objects are no longer supplying the same symbols.
