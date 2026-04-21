# 4. Audio y pantalla (LVGL)

## Audio — `NoAudioCodecSimplex`

Definido en `otto_pet_custom_board.cc` con pines de `config.h`:

- **Mic INMP441** (I2S): `WS`, `SCK`, `DIN` → GPIO 4, 5, 6.
- **Altavoz MAX98357A** (I2S): `BCLK`, `LRCK`, `DOUT` → GPIO 9, 8, 7.

Implementación: `main/audio/codecs/no_audio_codec.cc` (dos relojes / simplex).

## Pantalla — ST7789 sobre SPI3

Init en `OttoPetCustomBoard::InitializeSpi()` / `InitializeLcdDisplay()`:

- Bus SPI3: MOSI 13, CLK 12, etc. (ver `config.h`).
- Panel `esp_lcd` ST7789; objeto final: `SpiLcdDisplay`.

## LVGL

- `SpiLcdDisplay` constructor: relleno blanco por filas, enciende panel, `lv_init()`, caché de imágenes en PSRAM si aplica, `lvgl_port_init` con **afinidad a CPU1** en dual-core.
- UI base y emojis: tema LVGL (`LvglTheme`) + colección de emojis (ver doc 05).

## Barra de estado y hora

- `main/display/lvgl_display/lvgl_display.cc`: en estado idle, cada ~10 s intenta mostrar **HH:MM** si el año del sistema es ≥ 2025 (heurística de “hora ya sincronizada por SNTP”); si no, log de advertencia.

Esto **no** es la lógica Otto; solo cosmética de reloj en pantalla.
