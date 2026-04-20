#ifndef _BOARD_OTTO_PET_CUSTOM_CONFIG_H_
#define _BOARD_OTTO_PET_CUSTOM_CONFIG_H_

#include <driver/gpio.h>

/**
 * Placa genérica: ESP32-S3 + INMP441 (mic I2S) + MAX98357A (altavoz I2S) + ST7789 240×240
 * + 5× MG90S (adelante, izquierda, derecha, atrás, cola) + WebSocket (puerto 8080, /ws).
 *
 * IMPORTANTE (ESP32-S3 con PSRAM OCT como en sdkconfig.defaults.esp32s3):
 * - Los GPIO ~26–37 suelen estar ligados al bus SPI de la PSRAM / memoria externa.
 * - NO uses esos pines para servos ni para señales “rígidas”; forzar PWM ahí puede
 *   pelear con el bus y dañar la ESP o los módulos conectados.
 * - Los servos aquí están en 16,17,18,21,47 (fuera de ese bloque típico).
 *
 * Ajusta los GPIO en TU PCB según tu módulo concreto (datasheet del WROOM / N16R8).
 */

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
#define AUDIO_I2S_METHOD_SIMPLEX

#ifdef AUDIO_I2S_METHOD_SIMPLEX
/** INMP441: SCK→BCLK, WS→LRCK, SD→datos hacia la ESP */
#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6
/** MAX98357A: BCLK, LRCLK, DIN */
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_9
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_8
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7
#endif

#define BUILTIN_LED_GPIO        GPIO_NUM_48
#define BOOT_BUTTON_GPIO        GPIO_NUM_0

#define LAMP_GPIO               GPIO_NUM_NC

#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR true
#define DISPLAY_RGB_ORDER LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE 3

#define DISPLAY_MOSI_PIN      GPIO_NUM_13
#define DISPLAY_CLK_PIN       GPIO_NUM_12
#define DISPLAY_DC_PIN        GPIO_NUM_11
#define DISPLAY_RST_PIN       GPIO_NUM_10
#define DISPLAY_CS_PIN        GPIO_NUM_14
#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_15

/**
 * Orden firmware otto_pet: FRONT_LEFT, FRONT_RIGHT, REAR_LEFT, REAR_RIGHT, TAIL
 * Mapeo mecánico: adelante, derecha, izquierda, atrás, cola.
 */
#define OTTO_SERVO_GPIO_FRONT_LEFT   GPIO_NUM_17
#define OTTO_SERVO_GPIO_FRONT_RIGHT  GPIO_NUM_21
#define OTTO_SERVO_GPIO_REAR_LEFT    GPIO_NUM_18
#define OTTO_SERVO_GPIO_REAR_RIGHT   GPIO_NUM_47
#define OTTO_SERVO_GPIO_TAIL         GPIO_NUM_16

#endif
