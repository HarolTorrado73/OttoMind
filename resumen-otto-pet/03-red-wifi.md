# 3. Red WiFi

## Código principal

- `main/boards/common/wifi_board.cc`: `WifiBoard::StartNetwork()` crea `WifiManagerConfig` (prefijo SSID tipo **Xiaozhi** para AP de configuración), `WifiManager::Initialize`, registra callback de eventos WiFi y llama `TryWifiConnect()`.
- `TryWifiConnect()`: si `SsidManager` tiene redes guardadas → `StartStation()` + temporizador de timeout; si **no** hay SSID → tras 1,5 s entra en **modo configuración** (`StartWifiConfigMode()`).

## Aprovisionamiento

Con `CONFIG_USE_HOTSPOT_WIFI_PROVISIONING` (típico en tu `sdkconfig`):

- La ESP abre un **punto de acceso**; el móvil se conecta al WiFi del dispositivo y se usa la **URL del portal** que muestra la pantalla para introducir SSID/clave del router (2,4 GHz).

## Logs útiles

- `WifiBoard: Starting WiFi connection attempt`
- `WifiBoard: Connected to WiFi: <ssid>`
- `WifiBoard: WiFi config mode entered` (sin credenciales o timeout de conexión)

## Nota sobre `StartNetwork` en otto-pet-custom

Tras `WifiBoard::StartNetwork()` la placa hace `vTaskDelay(1000)` y arranca el **WebSocket** en 8080. Eso ocurre en la tarea **`net_init`**, no en `app_main`.
