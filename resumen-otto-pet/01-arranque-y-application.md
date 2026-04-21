# 1. Arranque y `Application`

## `main/main.cc`

- Inicializa **NVS** (`nvs_flash_init`; si hace falta, borra y reinicia NVS).
- Obtiene `Application::GetInstance()`, llama a `Initialize()` y luego `Run()` (bucle de eventos; no vuelve).

## `main/application.cc` — orden lógico en `Initialize()`

1. `Board::GetInstance()` (singleton de placa; en otto-pet ya se construyó el LCD, SPI, botón, etc.).
2. Estado del dispositivo: `kDeviceStateStarting`.
3. **Pantalla:** `display->SetupUI()`, mensaje de sistema con `SystemInfo::GetUserAgent()`.
4. **Audio:** `audio_service_.Initialize(codec)` y `Start()`; callbacks de cola / wake word / VAD.
5. **Otto pet** (solo si `CONFIG_BOARD_TYPE_OTTO_PET_CUSTOM` y ESP32-S3): `InitOttoPetModules()` → `behavior_init` + mapa de servos desde `config.h`.
6. **Reloj UI** periódico (`esp_timer` 1 s).
7. **MCP** herramientas comunes y de usuario.
8. **Callback de red** (`board.SetNetworkEventCallback`): notificaciones en pantalla y bits del `event_group_`.
9. **Red:** no llama `StartNetwork()` en la tarea principal; crea la tarea **`net_init`** (pila 12288, prioridad 5) que tras 20 ms ejecuta `Board::GetInstance().StartNetwork()` y se autodestruye. Motivo: `esp_wifi_init` apila muchas llamadas; tras un `Initialize()` largo la pila de `app_main` (8192 B) podía desbordarse y provocar `TG1WDT_SYS_RST`.
10. `display->UpdateStatusBar(true)`.

## `Application::Run()`

- Sube la prioridad de la tarea principal a **10**.
- Espera bits en `event_group_`: audio, red, estado, schedule, etc., y despacha manejadores.

## Callbacks Otto → UI / sonido

En el mismo archivo, namespace anónimo:

- `OttoEmotionCallback(emotion)` → `app.Schedule` → `display->SetEmotion(...)`.
- `OttoReactionCallback(reaction)` → sonidos OGG según reacción (salta `thinking` / `curious`).

Se registran con `behavior_set_callbacks` dentro de `InitOttoPetModules()`.
