# 8. Documentación y trazabilidad (procedimientos)

Complementa **`00-contexto-proyecto-y-asistentes.md`** (marco general) y **`07-conexiones-electricas-checklist.md`** (GPIO, potencia, bring-up eléctrico). Aquí: **cómo documentar** el montaje, las pruebas y el software para que ChatGPT / Cursor / tú futuro no tengan que adivinar.

---

## 1. Checklist de montaje (antes de soldar o energizar)

| Paso | Acción | Criterio de aceptación |
|------|--------|-------------------------|
| M1 | Lista de hilos **origen → destino → función** alineada con `xiaozhi-esp32/main/boards/otto-pet-custom/config.h` | Cero GPIO fuera del header o justificados en comentario |
| M2 | Revisar **silkscreen** de cada breakout (INMP441, MAX98357, LCD, MT3608, BMS) | Nombres de pines coinciden con la lista (WS/LRCK, etc.) |
| M3 | **Continuidad** y polaridad con multímetro **sin** alimentación | Sin cortos entre 5 V, 3V3 y GND |
| M4 | Inspección visual: soldaduras, virutas, dupont firmes | — |
| M5 | Estrategia **USB vs batería** (jumpers, diodos, PMIC) documentada | Saber qué pasa si conectas ambos |

---

## 2. Registro de pruebas (plantilla)

Copia la tabla por cada sesión de laboratorio; adjunta foto o nota al commit / carpeta de proyecto.

| Fecha | Etapa (ver 07 y 00) | Tensión / corriente observada | Resultado (OK / fallo) | Mitigación si falló |
|-------|---------------------|-------------------------------|-------------------------|----------------------|
|       | ej. “solo boost + 5V” | Vin Vbat, Vout boost | | |
|       | ej. “+ pantalla” | | | |
|       | ej. “+ 1 servo PWM” | Caída en +5V al mover | | |

---

## 3. Esquema lógico de potencia (una línea para el cuaderno)

```
LiPo 1S (+) ──► [BMS] ──► (+) ──► MT3608 IN ──► MT3608 OUT ≈5V ──┬──► servos V+
                                                                ├──► MAX98357 VCC (si 5V)
                                                                └──► pin 5V/VIN del devkit (si procede)
(-) batería ═══ GND común ═══ GND ESP ═══ GND servos ═══ GND audio ═══ GND LCD
```

Añade en el dibujo los **condensadores** previstos (boost, bulk servos, decoupling 3V3 ESP). Si usas **carga USB** además, dibuja el camino de corriente y el punto único de unión para no cargar la celda desde dos sitios sin control.

---

## 4. Notas de software (trazabilidad)

| Tema | Qué apuntar en el doc / commit |
|------|----------------------------------|
| Placa activa | `CONFIG_BOARD_TYPE_OTTO_PET_CUSTOM=y` y ruta `otto-pet-custom` |
| Red / WDT | Presencia de tarea **`net_init`** en `application.cc`; tamaño de pila si lo cambias |
| WiFi | Modo provisión (hotspot), credenciales en NVS, SSID de prueba |
| Emojis | `DEFAULT_EMOJI_COLLECTION` en `main/CMakeLists.txt` (`twemoji_32` vs `otto-gif`); assets OTA si aplica |
| Particiones / flash | Tamaño flash, tabla de particiones si afecta a assets |
| `menuconfig` | Captura o lista de opciones no obvias (`HTTPD_WS_SUPPORT`, PSRAM, etc.) |

---

## 5. Ideas de firmware (backlog conceptual)

Para discutir con un asistente antes de codificar; no sustituyen revisar el código actual.

1. **Tareas desacopladas:** I2S captura/reproducción y LVGL en hilos con prioridades y stacks dimensionados; evitar trabajo largo en callback de red en la tarea incorrecta.  
2. **Energía:** modos de bajo consumo cuando no hay interacción (si la placa y el producto lo permiten).  
3. **UI local:** menú LVGL para WiFi sin móvil (requiere diseño UX + almacenamiento de redes).  
4. **Assets:** regenerar pack con el generador oficial de assets / emojis y documentar la URL o versión del binario flasheado en partición assets.  
5. **Wake word / idioma:** parámetros en `menuconfig` y modelo SR usado.

---

## 6. Advertencias (no omitir en revisiones)

- **Ripple del boost** y **picos de servos** pueden provocar **brownout** o **ruido I2S**; mitigar con masa y condensadores antes de atribuir el fallo al “WiFi” o al “código”.  
- **Pines de flash/PSRAM** del módulo concreto: no reasignar sin datasheet.  
- **Corriente del MT3608:** no asumir amperaje de datasheet “ideal” en módulo genérico; medir o limitar movimiento concurrente.

---

## 7. Preguntas abiertas (rellenar; guían la siguiente iteración del doc)

- ¿Modelo exacto de celda (mAh, descarga máxima)?  
- ¿El devkit alimenta la ESP solo por **5 V** o también acepta **3V3** externo?  
- ¿Osciloscopio disponible para medir ripple en 5 V con servos en marcha?
