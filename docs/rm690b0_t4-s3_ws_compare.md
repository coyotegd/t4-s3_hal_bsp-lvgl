# RM690B0 + HAL/BSP-LVGL Comparison (ws_241 vs t4-s3)

This document compares the current implementations in:

- `ws_241_hal_bsp-lvgl` (Waveshare 2.41 AMOLED)
- `t4-s3_hal_bsp-lvgl` (LilyGo T4-S3)

It reflects the latest state after rotation/LVGL stabilization work.

---

## 1) Executive Summary

Both projects now follow the same core architecture for graphics:

1. **HAL owns hardware rotation (MADCTL).**
2. **BSP/LVGL updates logical resolution** (`lv_display_set_resolution`) and redraws.
3. **Display flush is asynchronous** (queue + worker task) so LVGL is not blocked on full SPI transfer.
4. **PSRAM draw buffers + partial render mode** are used for practical throughput.

### Bottom line

- **Capability parity is high** for display + rotation + LVGL path.
- **Differences are mostly hardware/peripheral-specific** and optional advanced features (callbacks, TE/VSYNC wiring, richer power APIs).
- Neither is universally “better”; each is best on its own board hardware.

---

## 2) Similar Hardware vs Dissimilar Hardware

## Similar

- Same MCU family: **ESP32-S3**
- Same display controller: **RM690B0**
- Same display data path concept: QSPI-style command wrapper + chunked pixel transfers
- Same software stack level: ESP-IDF + FreeRTOS + LVGL

## Dissimilar

- **Board routing/power** differ significantly
- **Touch IC differs**
  - ws_241: FT6336U
  - t4-s3: CST226SE
- **Power subsystem differs**
  - ws_241: TCA9554 + board-specific power latch behavior
  - t4-s3: SY6970 PMIC-centric management
- **Display calibration differs** (offsets and effective window dimensions)

Implication: driver structure can be shared philosophically, but constants/offsets and peripheral modules must stay board-specific.

---

## 3) Current Architecture Comparison

## 3.1 Display Driver (RM690B0)

Both now have:

- Async flush queue + worker task
- Chunked pixel write path
- Runtime rotation + width/height query
- Rotation-safe window offset handling

Key difference:

- `t4-s3` has a broader API surface (additional callbacks, extra utility/display control features).
- `ws_241` keeps a tighter API set focused on required behavior.

## 3.2 BSP + LVGL Layer

Both now use the same critical pattern:

- On rotation event:
  - update LVGL resolution with current hardware width/height
  - invalidate active screen
- Do **not** apply LVGL software rotation transform on top of hardware MADCTL

That was the primary fix for skew/double-rotation behavior.

## 3.3 Tasking/Sync Model

Both use:

- FreeRTOS LVGL task
- Async display flush completion callback into `lv_display_flush_ready`
- Recursive LVGL lock approach (or equivalent lock-safe behavior)

---

## 4) Performance/Capability: Are they comparable now?

For the shared display pipeline (LVGL + RM690B0): **yes, broadly comparable**.

## Why comparable

- Both use async flush worker model
- Both use PSRAM draw buffers + partial rendering
- Both use RGB565 with byte swap handling
- Both have rotation-resolution synchronization

## Where one can be stronger

- `t4-s3` has more mature “extras”:
  - richer callback wiring
  - optional VSYNC/TE coordination hooks
  - broader system feature integration
- `ws_241` now matches core display path quality, but has a smaller optional-feature surface.

---

## 5) Rotation Model (Current, ws_241)

Startup default: **Rotation 0**.

| Rotation | USB Side | Orientation | Degrees |
| :--- | :--- | :--- | :--- |
| 0 | Bottom | Landscape | 0° |
| 1 | Right | Portrait | 90° |
| 2 | Top | Landscape (inverted) | 180° |
| 3 | Left | Portrait (inverted) | 270° |

This mapping is now stable at startup and through full button cycling.

---

## 6) Practical “Better/Worse” Answer

- If the goal is **board-specific reliability and clean architecture**, both are in good shape.
- If the goal is **maximum framework/feature breadth**, `t4-s3` still has more non-essential convenience features.
- If the goal is **core display correctness/perf on ws_241 hardware**, current ws_241 implementation is now at parity where it matters.

---

## 7) Recommended Unified Direction (for both repos)

To keep long-term parity:

1. Keep shared rotation policy: **HW MADCTL + LVGL resolution sync only**.
2. Keep async flush worker pattern in both.
3. Keep partial render + PSRAM defaults.
4. Keep board-specific constants in board-local modules (offsets, touch transforms, power sequencing).
5. Maintain one quick-reference matrix (see companion quick-reference doc).

---

## 8) Quick Reference Link

See: [RM690B0 Quick Reference](rm690b0_t4-s3_ws_qr.md)
