# 7. Interconexión eléctrica y bring-up

**Alcance:** placa lógica **otto-pet-custom** (ESP32-S3, PSRAM octal, ST7789 240×240, INMP441 + MAX98357A simplex, cinco servos).  
**Fuente de verdad de señales:** `xiaozhi-esp32/main/boards/otto-pet-custom/config.h` (GPIO frente a firmware).

Este apartado es un **procedimiento repetible**: mismo orden, mismas comprobaciones, cada vez que cambies cableado o revisión de hardware.

---

## Conexiones — mapeo GPIO (tabla de cableado)

> Si el archivo se ve “vacío” en el visor, **ábrelo como texto** (`Ctrl+Shift+V` / vista sin preview) o en otro editor: algunos visores de Markdown no pintan tablas o dejan el bloque `mermaid` en blanco.

### Alimentación y masa

| Red | Conexión típica | Nota |
|-----|-----------------|------|
| 3V3 | 3V3 del módulo ESP → VCC de INMP441, MAX98357A, ST7789 (según breakout) | No alimentes el conjunto de servos desde este rail. |
| 5 V | **Fuente externa** 5 V → **V+** de cada servo (bus común MG90S) | El **GND** de esa fuente debe unirse al **GND de la ESP**. |
| GND | Bus común: ESP ↔ audio ↔ LCD ↔ servos ↔ negativo 5 V | Sin esto, I2S y WiFi degradan o la ESP reinicia. |

### INMP441 (micrófono I2S)

| Función (firmware) | GPIO ESP32-S3 | Nombres habituales en breakout |
|--------------------|---------------|--------------------------------|
| WS / LRCK         | **4**         | WS, L/R, LRCLK |
| SCK / BCLK        | **5**         | SCK, BCLK |
| Datos mic → ESP   | **6**         | SD, DOUT, DATA |

Añade **3V3** y **GND** del INMP441 según el silkscreen del módulo.

### MAX98357A (altavoz I2S)

| Función (firmware) | GPIO ESP32-S3 | Nombres habituales en breakout |
|--------------------|---------------|--------------------------------|
| BCLK              | **9**         | BCLK |
| LRCLK             | **8**         | LRC, LRCLK, WS |
| Datos ESP → DAC   | **7**         | DIN |

Añade **3V3**, **GND** y, si el módulo lo trae, **GAIN** a tierra o V según el volumen deseado (datasheet del breakout).

### Pantalla ST7789 240×240 (SPI3 en firmware)

| Función | GPIO ESP32-S3 |
|---------|---------------|
| MOSI    | **13** |
| CLK     | **12** |
| DC      | **11** |
| RST     | **10** |
| CS      | **14** |
| Backlight | **15** |

Añade **3V3**, **GND** y, si aplica, **BL** o `LED` al pin de backlight indicado.

### LED y botón (placa)

| Función | GPIO |
|---------|------|
| LED     | **48** |
| BOOT    | **0** |

### Servos MG90S (PWM; **5 V y GND en bus externo**)

Orden en firmware `servo_map`: *front_left, front_right, rear_left, rear_right, tail*.

| Eje / rol      | Señal PWM (GPIO) |
|----------------|------------------|
| Front left     | **17** |
| Front right    | **21** |
| Rear left      | **18** |
| Rear right     | **47** |
| Tail           | **16** |

Cada servo: **señal** → GPIO de la tabla; **V+** → 5 V común; **GND** → GND común (misma estrella que la ESP).

**UART consola (ESP32-S3):** en muchos módulos los pines **43 / 44** son el USB-Serial/JTAG del log. No los reasignes a periféricos si usas ese monitor; en esta `config.h` no se usan para LCD/audio/servos.

---

## 1. Documentación previa al cableado

| Acción | Detalle |
|--------|---------|
| Lista de hilos | Tabla *origen → destino → función* (nombre lógico del pin en la ESP, no solo color del dupont). |
| Cruce con firmware | Cada GPIO debe existir en `config.h`; si el PCB difiere, **se corrige el header y se recompila**, no al revés. |
| Revisión de breakout | Los módulos comerciales permutan etiquetas (`LRC` vs `WS`, etc.); la referencia es el **datasheet del PCB concreto**, no el genérico del chip. |

---

## 2. Restricciones eléctricas y de integridad de señal

| Tema | Criterio | Motivo técnico |
|------|-----------|----------------|
| Referencia de masa | **GND común** entre ESP32-S3, INMP441, MAX98357A, ST7789 y retorno de la **fuente de 5 V de servos** | Sin masa compartida: corrientes de retorno por caminos imprevistos → ruido en I2S, UART y ADC, resets erráticos. |
| Servos | **5 V** en bus de servo; **no** alimentar MG90S desde el regulador 3V3 de la placa de desarrollo | Picos de arranque y stall superan con holgura lo que el LDO de la ESP puede entregar sin caída de tensión. |
| Nivel lógico PWM | Señal **3,3 V** desde GPIO hacia entrada del servo (típico en hobby) | Compatible con umbrales CMOS del driver del servo; evitar 5 V en GPIO no tolerantes. |
| PSRAM / bus octal | Evitar GPIO **~26–37** para PWM o cargas agresivas en módulos con memoria externa multiplexada | Esas líneas suelen ir al interface SPI/OPI; interferencia eléctrica o uso incorrecto compromete estabilidad de memoria y puede dañar periféricos. |
| USB | Mantener alimentación USB dentro de especificación durante pruebas | Brownout bajo carga simultánea (pantalla + WiFi + audio) ya se ve en campo; servos deben estar en rail **externo**. |

---

## 3. Secuencia de bring-up (integración incremental)

Cada etapa valida un subconjunto antes de acoplar el siguiente; reduce el radio de búsqueda ante fallos.

```mermaid
flowchart LR
  A[USB: boot + monitor] --> B[Pantalla + backlight]
  B --> C[Audio: I2S mic y/o altavoz]
  C --> D[WiFi / aplicación]
  D --> E[Servos: uno a uno]
```

| Paso | Verificación mínima |
|------|---------------------|
| 1 — Solo USB | Log limpio hasta `app_main`; sin `TG1WDT` ni brownout. |
| 2 — Display | Inicialización `LcdDisplay` / LVGL; imagen estable, sin parpadeos de reset. |
| 3 — Audio | Trazas `NoAudioCodec` / codec; prueba de silencio o tono según firmware disponible. |
| 4 — Red | Asociación STA o modo configuración; tráfico TLS/MQTT según despliegue. |
| 5 — Actuadores | Un servo por vez en PWM; comprobar consumo y ausencia de reinicios al mover carga. |

---

## 4. Diagnóstico rápido ante anomalías

| Síntoma | Hipótesis prioritaria | Acción |
|---------|----------------------|--------|
| Reinicio al mover servo | Caída de tensión / pico en 5 V o GND | Desacoplar servos del mismo USB; medir 3V3 y 5 V bajo carga; cortar tracción mecánica para reducir corriente. |
| I2S con ruido o clipping | Masa deficiente o cable largo sin twist | Acortar hilos, unir masas en un punto, revisar `BCLK`/`LRCK`/`DIN`/`DOUT` frente al breakout. |
| WiFi inestable tras añadir hardware | EMC o alimentación | Alejar antena de bucles de corriente de servos; ferritas opcionales en alimentación de motor. |

---

## 5. Bibliografía mínima recomendada

- **Espressif:** *ESP32-S3 Series Datasheet* y *Hardware Design Guidelines* (pinout del módulo WROOM/WROOM-1 que montes).
- **Micrófono:** hoja de datos del **módulo** INMP441 (no solo del chip).
- **Audio salida:** hoja del **módulo** MAX98357A (I2S, ganancia, filtros).
- **Pantalla:** init ST7789 según tu PCB (orden RGB, inversión, offsets ya fijados en software).

---

## 6. Trazabilidad

- Versión de firmware y **commit** o etiqueta cuando congeles un cableado “aprobado”.
- Fotografía o esquemático del cableado final junto a la tabla de hilos.

*Documento de apoyo a ingeniería; no sustituye normativa de seguridad eléctrica ni certificación de producto.*
