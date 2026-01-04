# RM690B0 Driver Implementation Notes

## Custom QSPI Protocol
The RM690B0 on the LilyGo T4-S3 uses a non-standard QSPI protocol to wrap commands and data. This is likely due to the specific interface configuration (IM[1:0]=10) and the controller's internal logic.

### Protocol Details
- **Write Command**: 
  - Opcode: `0x02` (1-bit)
  - Address: `0x00CC00` (1-bit), where `CC` is the command byte.
  - Data: Parameters (1-bit).
  - Mode: 1-1-1 (Standard SPI).

- **Write Data (Pixels)**:
  - Opcode: `0x32` (1-bit)
  - Address: `0x002C00` (1-bit), where `0x2C` is the RAMWR command.
  - Data: Pixel Data (4-bit QSPI).
  - Mode: 1-1-4 (Quad Page Program style).

## Driver Configuration
- **SPI Mode**: QIO (`SPI_TRANS_MODE_QIO`) is used for pixel data to enable 4-bit transfer.
- **CS Control**: Hardware CS (`PIN_NUM_QSPI_CS`) is used to allow the SPI driver to manage Chip Select, which is critical for DMA and queued transactions.
- **Max Transfer Size**: Set to 64KB (`65536`) to accommodate the LVGL draw buffer (~36KB) in a single DMA transaction.

## Async Flush
The driver implements `rm690b0_flush_async` using `spi_device_queue_transmit`. This allows the CPU to return to LVGL and render the next chunk while the SPI peripheral transmits the current chunk via DMA.
- **Safety**: The implementation relies on LVGL's single-threaded rendering loop. `flush_cb` is not called again until `lv_display_flush_ready` is invoked in the transaction callback.
- **Static Structures**: `spi_transaction_ext_t` and context structures are static to persist during the async transfer. This is safe only because of the serialized access guaranteed by LVGL.

## Power Sequence
1. **GPIO 9 (Power Enable)**: Must be set HIGH to power the display and PMIC rails.
2. **Reset**: Active Low pulse (GPIO 13).
3. **Initialization**: Standard SWRESET, SLPOUT, DISPON sequence.
