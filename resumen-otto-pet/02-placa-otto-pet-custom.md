# 2. Placa `otto-pet-custom`

## Archivos

| Archivo | Rol |
|---------|-----|
| `main/boards/otto-pet-custom/config.h` | **Única fuente de verdad** de GPIO: audio I2S simplex, LCD SPI, servos, LED, botón. |
| `main/boards/otto-pet-custom/otto_pet_custom_board.cc` | Clase `OttoPetCustomBoard` : `WifiBoard`; init SPI3 + ST7789 + botón + lámpara (NC); audio `NoAudioCodecSimplex`; `StartNetwork()` llama a `WifiBoard::StartNetwork()`, espera 1 s y arranca `WebSocketControlServer` en puerto **8080** (`/ws`). |
| `main/boards/otto-pet-custom/config.json` | Target `esp32s3`, `sdkconfig_append` (tipo de placa, `CONFIG_HTTPD_WS_SUPPORT`). |
| `main/boards/otto-pet-custom/websocket_control_server.*` | HTTP + WebSocket para control/MCP JSON remoto. |

## Herencia

`OttoPetCustomBoard` → `WifiBoard` → red WiFi estándar del proyecto (`WifiManager` vía `esp-wifi-connect`).

## Pantalla usada

`SpiLcdDisplay` (clase en `lcd_display.cc`), **no** `OttoEmojiDisplay` (esa es la variante `otto-robot` con más lógica Otto en pantalla).

## Selección en build

En `main/CMakeLists.txt`, rama `CONFIG_BOARD_TYPE_OTTO_PET_CUSTOM`:

- `BOARD_TYPE` = `otto-pet-custom` (carpeta de assets / recursos embebidos según convención del proyecto).
- Fuentes e iconos por defecto para esa placa.
- `DEFAULT_EMOJI_COLLECTION` = **`twemoji_32`** (emojis estáticos compilados; ver documento 05).

## Kconfig

En `main/Kconfig.projbuild` debe existir la opción de placa que activa `BOARD_TYPE_OTTO_PET_CUSTOM` (nombre visible en `menuconfig`).
