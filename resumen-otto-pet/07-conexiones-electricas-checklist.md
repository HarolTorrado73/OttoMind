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
| 3V3 | 3V3 del módulo ESP → VCC de INMP441, MAX98357A, ST7789 (según breakout) | No alimentes MG90S desde 3V3. |
| 5 V servos | **5 V con corriente adecuada** (USB, batería+MT3608, etc.) | Ver apartado siguiente: USB de pared, **LiPo 1S + elevador MT3608**, o buck desde VBUS. |
| GND | Bus común: ESP ↔ audio ↔ LCD ↔ servos ↔ retorno del mismo rail que alimenta los servos | Sin masa común, fallos intermitentes y reinicios. |

#### Proyecto compacto: qué hacer si no quieres un bloque de alimentación voluminoso

Lo que se evita no es “tener 5 V”, sino **intentar mover varios MG90S solo con el 3V3 o el LDO de la ESP** (eso sí rompe el proyecto pequeño por resets y calor).

Opciones **pequeñas** (elige una según tu caja):

1. **Cargador USB de pared** (cubo pequeño) + cable USB a la placa: si tu PCB o módulo entrega **5 V del bus USB** a un pad `VBUS`/`5V` y la corriente nominal del cargador es holgada (p. ej. ≥ 2 A), muchos montajes alimentan **servos en ese mismo 5 V** y la ESP por el regulador de la placa, con **GND común**. Sigue siendo “una sola fuente”, no un segundo transformador en mesa.
2. **Módulo step-down mini** (tipo buck en PCB de 2×2 cm) desde el mismo 5 V USB hacia un bus corto “solo servos”: ocupa poco y desacopla picos de los servos del rail que ve el regulador de la ESP.
3. **Solo USB al PC**: suele limitar a **~500 mA**; con pantalla + WiFi + audio ya queda poco margen. Con **uno o dos** microservos y **carga mecánica mínima** a veces basta para prototipo; con **cinco MG90S** en movimiento realista es fácil el brownout. Si insistes en este modo: mueve servos de **uno en uno** en pruebas, reduce velocidad/carga en mecánica y asume que puede ser inestable.

4. **Batería Li-ion / LiPo (1S) + módulo elevador MT3608** (u otro boost ajustable): encaja muy bien en un cuerpo pequeño; la electrónica es la habitual en portátiles / wearables.

   | Bloque | Función |
   |--------|---------|
   | Celda 1S | Tensión nominal ~3,7 V; operativa aprox. **3,0 V–4,2 V** (descarga ↔ carga completa). |
   | **Protección BMS** (p. ej. DW01 + MOSFET en placa “18650 con protección”) | Corta sobredescarga y sobrecorriente; **no** omitas en LiPo suelta. |
   | **Carga** (TP4056, IP5306, etc.) | Solo con celda protegida o módulo que ya integre protección; respeta corriente de carga recomendada por el fabricante de la celda. |
   | **MT3608** (boost) | Entrada desde el **+ de la batería** (tras BMS si aplica); salida regulada típicamente a **5,0–5,2 V** para bus de servos y, si tu placa lo admite, al pin **5 V / VIN** del devkit para que el LDO interno siga alimentando la ESP a 3V3. |
   | **GND** | Negativo de celda = GND del MT3608 = GND de la ESP = GND de servos y periféricos. |

   **Consideraciones de diseño:**

   - El MT3608 es **elevador** (Vout > Vin aprox.); con la celda alta (~4,1 V) sigue funcionando si ajustas bien la salida; cerca del **corte por bajo** (~3,3 V) el boost exige más corriente de entrada para el mismo wattio salida → más calor y más caída; conviene **parar servos agresivos** o avisar batería baja por software si un día lo cableas.
   - Corriente: los módulos MT3608 de mercado suelen anunciarse ~**2 A** salida con **asterisco** (inductor, disipación, entrada baja). Cinco MG90S en **stall simultáneo** pueden pedir mucho más; en robot reducido suele bastar con **no bloquear todos a la vez** y mecánica ligera, o repartir en el tiempo los picos.
   - **Ruido conmutado**: el boost genera ripple; mantén **condensadores** en la salida del módulo y trazos cortos; el audio I2S es sensible a masa sucia → estrella de GND y retornos cortos ayudan.
   - **USB y batería a la vez**: si quieres cargar mientras usas el aparato, usa un esquema con **gestión de fuente** (idealmente un PMIC o módulo “charge+boost” documentado) para no poner 5 V USB en paralelo ingenuo con la celda.

En resumen: no necesitas una fuente “de laboratorio grande”; necesitas **5 V (o 3V3 estable para la ESP) con corriente suficiente, GND común y protección de batería** si vas por LiPo. El **MT3608** es una opción razonable y compacta para subir 1S → 5 V para servos y alimentación de placa.

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

### Servos MG90S (PWM; **5 V + GND en bus común**, ver alimentación arriba)

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
| USB | Cargador o cable con **corriente suficiente** si compartes 5 V con servos | Brownout si el conjunto supera lo que entrega el puerto o el polyfuse; preferir **cargador de pared ≥ 2 A** antes que USB de portátil para cinco MG90S. |

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
