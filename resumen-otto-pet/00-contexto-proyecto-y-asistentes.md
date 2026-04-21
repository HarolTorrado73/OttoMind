# Contexto del proyecto (Xiaozhi / OttoMind) y flujo con asistentes

Documento de referencia para alinear a **ChatGPT**, **Cursor** u otros asistentes con el mismo marco técnico. Las citas web del borrador original se omiten aquí; contrasta siempre con el **código y `sdkconfig` reales** del repo.

---

## Notas del mantenedor (correcciones frente a textos genéricos)

1. **MT3608 y corriente:** Los módulos MT3608 de mercado suelen estar en el orden de **~1–2 A útiles** salida (según inductor, disipación y Vin). Hablar de **“regulador 5 V > 4 A”** como si el MT3608 solo lo garantizara es **optimista**; para picos de **cinco MG90S** en stall simultáneo a menudo hace falta **otra topología**, **banco de condensadores**, **no exigir todos los motores a la vez**, o **alimentación USB/cargador de pared** con margen. Ver también `07-conexiones-electricas-checklist.md`.
2. **GPIO y PSRAM:** En **otto-pet-custom** la advertencia práctica del firmware está en `xiaozhi-esp32/main/boards/otto-pet-custom/config.h` (evitar GPIO ligados al bus de **PSRAM octal**, típicamente el bloque **~26–37** en muchos WROOM N8R8/N16R8). El rango exacto depende del **módulo**; la fuente autoritativa es el **datasheet del módulo ESP32-S3** que montes, no un resumen genérico.
3. **WiFi y WDT:** En esta línea de trabajo se mitigó el arranque de red moviendo `StartNetwork()` a una **tarea dedicada** con más pila (`application.cc`). Comprueba en tu rama que ese cambio sigue presente.
4. **Emojis estáticos:** Para `CONFIG_BOARD_TYPE_OTTO_PET_CUSTOM`, `main/CMakeLists.txt` fija `DEFAULT_EMOJI_COLLECTION` a **`twemoji_32`** (imágenes fijas). Animación GIF implica otra colección (`otto-gif`) y/o assets en el generador de recursos.

---

## Qué es Xiaozhi-ESP32 en este repo

Firmware de **asistente de voz** para ESP32 (aquí **ESP32-S3** con **PSRAM**): wake word, audio **Opus**, protocolo con servidor (p. ej. **WebSocket** o **MQTT** según build), **MCP** para herramientas en dispositivo, **LVGL** en LCD (p. ej. ST7789 240×240), multilenguaje, muchas variantes de placa.

**OttoMind** añade o integra la variante **otto-pet-custom**: servos, FSM de comportamiento, `config.h` con GPIO, WebSocket local en **8080**, etc.

Herramientas recomendadas upstream: **ESP-IDF** (≥ 5.4 en documentación oficial; tu entorno puede ser 5.5.x), **Cursor** o VSCode con extensión ESP-IDF.

---

## Electrónica de potencia y cableado (resumen)

- **LiPo 1S:** nominal ~3,7 V; carga hasta ~4,2 V. Usar **BMS** (protección sobredescarga / sobrecorriente) y **módulo de carga** acorde a la celda (mAh, C-rate de carga).
- **Boost MT3608 (o similar):** eleva Vin a **~5 V** para servos y, si la placa lo admite, al rail de entrada del regulador que alimenta la ESP. Colocar **cerámicos** (p. ej. 22 µF X5R/X7R cerca de Vin/Vout según datasheet del integrado concreto) y, si hace falta, **electrolítico** y/o **LC** para ripple en audio.
- **3,3 V:** La ESP y muchos breakouts digitales quieren 3V3 estable; **decoupling** local (100 nF + varios µF) en la ESP y líneas sensibles.
- **Servos MG90S:** picos de corriente altos en arranque/stall; **condensador grueso** entre +5V y GND cerca del bloque de servos y **GND en estrella** o retornos cortos. Ver consumo real con multímetro/shunt si es posible.
- **I2S / MAX98357A:** sensible a **GND y ripple** compartidos con servos; un solo punto de unión de masas y filtrado ayuda.
- **USB + batería:** evitar paralelar ingenuamente; usar esquema con **selector de fuente** o PMIC de carga + boost documentado.

Detalle de **GPIO y tabla de cableado:** `07-conexiones-electricas-checklist.md`.

---

## Bring-up (orden de pruebas)

1. Continuidad y polaridad **sin** energía.  
2. Primera energía: solo placa / boost, **sin** servos o con uno.  
3. Medir **5 V y 3V3** bajo carga leve; comprobar boot por UART.  
4. Añadir **pantalla**, luego **audio**, luego **WiFi** (ver logs).  
5. **Servos** de uno en uno; observar si reaparecen brownouts o WDT.

Riesgos: brownout con WiFi + servos; pines prohibidos por **flash/PSRAM**; **pila insuficiente** en tareas de red o UI.

---

## Firmware: piezas que encajan

| Área | Dónde mirar |
|------|-------------|
| Arranque y red | `main/main.cc`, `main/application.cc` (incl. tarea `net_init`), `main/boards/common/wifi_board.cc` |
| Placa Otto pet | `main/boards/otto-pet-custom/config.h`, `otto_pet_custom_board.cc` |
| Emojis / LVGL | `main/display/lcd_display.cc` (`SetEmotion`), `main/CMakeLists.txt` (`DEFAULT_EMOJI_COLLECTION`), `main/assets.cc` (emoji_collection en JSON de assets) |
| Servos / FSM | `main/otto_pet/servo_control.c`, `behavior.c`, `application.cc` (`InitOttoPetModules`, callbacks) |
| MCP / herramientas | `main/mcp_server.cc` y registros en `application.cc` |

---

## Cómo pueden colaborar ChatGPT y Cursor

- **ChatGPT:** bueno para **dimensionado**, listas de comprobación, explicar ripple/BMS, reformular requisitos, detectar incoherencias en un texto de diseño. Peor para **pinout exacto** sin datasheet del módulo concreto.  
- **Cursor:** bueno para **editar el repo**, buscar símbolos, alinear `config.h` con firmware, parches, `menuconfig` documentado en comentarios.  
- **Flujo sugerido:** tú defines objetivo → ChatGPT (o este doc) afina **especificación** → Cursor **implementa** y deja rastro en git → tú actualizas `resumen-otto-pet` si cambia el hardware.

---

## Preguntas pendientes típicas (rellenar en tu copia)

- Modelo exacto de **módulo ESP32-S3** (WROOM-1 N16R8, etc.).  
- **Capacidad y C** de la LiPo; modelo del **cargador/BMS**.  
- Si el devkit expone **5 V** desde USB y si se puede inyectar 5 V desde el boost **sin** conflictos con el cargador.

---

## Documentación operativa (detalle)

Procedimientos de **montaje, registro de pruebas, esquema de potencia y notas de software** están en **[08-documentacion-y-trazabilidad.md](08-documentacion-y-trazabilidad.md)** para no duplicar el checklist eléctrico del `07`.
