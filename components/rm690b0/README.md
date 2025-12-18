# RM690B0 Display Driver Component

This component handles the low-level communication with the RM690B0 AMOLED controller using the ESP32-S3 SPI peripheral.

## Protocol Details (The "No DCX" Implementation)

The RM690B0 on the LilyGo T4-S3 is connected via a QSPI interface but lacks a standard Data/Command (D/C) pin. Instead, it uses a specific SPI Opcode wrapper protocol found in the `Arduino_ESP32QSPI` library.

### Command Transmission
Commands are sent using the standard SPI "Page Program" Opcode `0x02`.
- **Opcode:** `0x02` (8-bit)
- **Address:** 24-bit. The command byte is embedded in the middle byte.
  - Format: `0x00` `[CMD]` `0x00`
- **Data:** Follows immediately in Standard SPI mode (1-bit).

```c
// Implementation in rm690b0.c
spi_transaction_ext_t t = {
    .base.cmd = 0x02,
    .base.addr = ((uint32_t)cmd) << 8, // Command embedded in address
    .base.flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR,
    // ...
};
```

### Pixel Data Transmission
Pixel data is sent using the "Quad Page Program" Opcode `0x32`.
- **Opcode:** `0x32` (8-bit)
- **Address:** Fixed to `0x002C00` (24-bit).
  - `0x2C` corresponds to the `RAMWR` (Memory Write) command.
- **Data:** Sent in Quad Mode (4-bit).

```c
// Implementation in rm690b0.c
spi_transaction_ext_t t = {
    .base.cmd = 0x32,
    .base.addr = 0x002C00, // Fixed Address for RAMWR
    .base.flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_MODE_QIO,
    // ...
};
```

## Initialization Sequence
The initialization sequence is critical and was ported from the `LilyGo-AMOLED-Series` library.
1. **Power On:** GPIO 9 High (PMIC Enable).
2. **Reset:** GPIO 13 High -> Low -> High.
3. **Unlock:** Send `0xFE` `0x20` to unlock manufacturer commands.
4. **Configuration:** Set MIPI off, SPI RAM access, SWIRE settings, Pixel Format (`0x55` for 16-bit), etc.
5. **Sleep Out & Display On:** Standard `0x11` and `0x29` commands.
