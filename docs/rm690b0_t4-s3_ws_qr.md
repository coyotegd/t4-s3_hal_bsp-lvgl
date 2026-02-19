# RM690B0 HAL/BSP-LVGL Quick Reference

Full detail: [RM690B0 + HAL/BSP-LVGL Comparison](rm690b0_t4-s3_ws_compare.md)

---

## A) Similar vs Dissimilar Hardware

| Topic | ws_241 (Waveshare) | t4-s3 (LilyGo) | Impact |
| :--- | :--- | :--- | :--- |
| MCU class | ESP32-S3 | ESP32-S3 | Similar runtime model |
| Display IC | RM690B0 | RM690B0 | Same core display logic |
| Touch IC | FT6336U | CST226SE | Different input transform code |
| Power path | TCA9554 + board latch | SY6970-centric | Different HAL responsibilities |
| Display calibration | ws-specific offsets/window | t4-specific offsets/window | Must remain board-tuned |

---

## B) Current Software Architecture (Both)

| Layer | ws_241 | t4-s3 | Status |
| :--- | :--- | :--- | :--- |
| HAL rotation | MADCTL in display driver | MADCTL in display driver | Same pattern |
| BSP/LVGL rotation | `lv_display_set_resolution` + invalidate | Same | Same pattern |
| LVGL flush | async queue/worker | async queue/worker | Same pattern |
| Buffers | PSRAM + PARTIAL render mode | PSRAM + PARTIAL render mode | Same pattern |
| Byte order | RGB565 swap before flush | RGB565 swap before flush | Same pattern |

---

## C) Capability Snapshot

| Capability | ws_241 | t4-s3 |
| :--- | :--- | :--- |
| Stable 0/1/2/3 rotation | Yes | Yes |
| Startup orientation correctness | Yes | Yes |
| Async display updates | Yes | Yes |
| LVGL integration quality | High | High |
| Extra callback ecosystem (VSYNC/error/power depth) | Moderate | Higher |

---

## D) ws_241 Rotation Map (Current)

Startup default: **Rotation 0**

| Rotation | USB Side | Orientation | Degrees |
| :--- | :--- | :--- | :--- |
| 0 | Bottom | Landscape | 0° |
| 1 | Right | Portrait | 90° |
| 2 | Top | Landscape (inverted) | 180° |
| 3 | Left | Portrait (inverted) | 270° |

---

## E) Practical Verdict

- For the **display/LVGL pipeline**, ws_241 and t4-s3 are now **comparably strong**.
- `t4-s3` still has more peripheral-oriented extras.
- `ws_241` is now aligned on the critical performance/correctness decisions.
