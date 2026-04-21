# Resumen arquitectura — Otto pet (xiaozhi-esp32)

Esta carpeta documenta **cómo encajan las piezas** del firmware para la placa **otto-pet-custom** (ESP32-S3), para consulta rápida en futuras sesiones y para no depender solo de la memoria.

| Documento | Contenido |
|-----------|-----------|
| [01-arranque-y-application.md](01-arranque-y-application.md) | `app_main`, `Application`, tarea `net_init`, orden de init |
| [02-placa-otto-pet-custom.md](02-placa-otto-pet-custom.md) | `otto_pet_custom_board.cc`, `config.h`, WebSocket 8080 |
| [03-red-wifi.md](03-red-wifi.md) | `WifiBoard`, credenciales NVS, modo hotspot |
| [04-audio-y-pantalla.md](04-audio-y-pantalla.md) | Simplex I2S, `SpiLcdDisplay`, LVGL, barra de estado / hora |
| [05-emojis-estaticos-y-animados.md](05-emojis-estaticos-y-animados.md) | Por qué ves Twemoji fijos; rutas hacia GIF / `otto-robot` |
| [06-servos-y-comportamiento.md](06-servos-y-comportamiento.md) | `servo_control`, `behavior`, callbacks desde `application.cc` |
| [07-conexiones-electricas-checklist.md](07-conexiones-electricas-checklist.md) | Orden seguro de cableado, GND, PSRAM, alimentación servos |

**Ruta del firmware:** `OttoMind/xiaozhi-esp32/`  
**Placa Kconfig:** `CONFIG_BOARD_TYPE_OTTO_PET_CUSTOM` → `main/boards/otto-pet-custom/`

Si actualizas GPIO o flujo de red, conviene **tocar también el apartado correspondiente** en estos `.md` para que el resumen siga siendo verdad.
