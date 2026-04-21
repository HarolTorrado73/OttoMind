# 6. Servos y comportamiento (FSM Otto)

## `main/otto_pet/servo_control.c`

- **LEDC** baja velocidad, timer 0, **50 Hz** (servo estándar), resolución 14 bit.
- Canales 0..4 → cinco servos; `servo_control_init` guarda `otto_servo_config_t` por servo (GPIO, límites, neutro, invert).
- `servo_set_angle` / `servo_get_angle`: convierte ángulo a duty y actualiza LEDC.

## `main/otto_pet/behavior.c`

- Cola FreeRTOS + tarea **`otto_fsm`** (prioridad 5): eventos STT, LLM, timers thinking/idle.
- Tarea **`otto_anim`** (prioridad 4): cada `CONFIG_OTTO_ANIMATION_INTERVAL_MS` (~80 ms) recalcula pose, interpola hacia objetivos (`lerp`) y llama `apply_pose_from_current()` → cinco `servo_set_angle`.
- Estados: idle, listening, thinking, happy, sad, excited; callbacks C registrados desde `application.cc` para emoción en pantalla y sonido.

## `main/otto_pet/emotions.c` / `emotions.h`

- Hooks o utilidades de emociones si el proyecto las usa (revisar includes desde `behavior` o MCP).

## Integración

- `application.cc`: `InitOttoPetModules()` solo con Kconfig de placa Otto pet + S3; `behavior_set_callbacks` enlaza UI y audio.
- **Orden firmware** de servos en el array: `FRONT_LEFT`, `FRONT_RIGHT`, `REAR_LEFT`, `REAR_RIGHT`, `TAIL` — debe coincidir con mecánica y `config.h`.

## Kconfig (intervalos / tiempos)

Prefijos `CONFIG_OTTO_*` en `main/Kconfig.projbuild` (thinking min/max, idle timeout, intervalo animación, etc.).
