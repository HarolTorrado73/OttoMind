#ifndef OTTO_PET_SERVO_CONTROL_H
#define OTTO_PET_SERVO_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OTTO_SERVO_COUNT 5

typedef enum {
    OTTO_SERVO_FRONT_LEFT = 0,
    OTTO_SERVO_FRONT_RIGHT,
    OTTO_SERVO_REAR_LEFT,
    OTTO_SERVO_REAR_RIGHT,
    OTTO_SERVO_TAIL,
} otto_servo_id_t;

typedef struct {
    int gpio_num;
    int min_angle;
    int max_angle;
    int neutral_angle;
    bool invert;
} otto_servo_config_t;

esp_err_t servo_control_init(const otto_servo_config_t config[OTTO_SERVO_COUNT]);
esp_err_t servo_set_angle(otto_servo_id_t servo, int angle_deg);
int servo_get_angle(otto_servo_id_t servo);

#ifdef __cplusplus
}
#endif

#endif  // OTTO_PET_SERVO_CONTROL_H
