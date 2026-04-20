#include "servo_control.h"

#include <driver/ledc.h>
#include <esp_check.h>
#include <esp_log.h>

#define TAG "otto_servo"

#define OTTO_SERVO_FREQ_HZ            50
#define OTTO_SERVO_TIMER_RESOLUTION   LEDC_TIMER_14_BIT
#define OTTO_SERVO_MIN_PULSE_US       500
#define OTTO_SERVO_MAX_PULSE_US       2500

static bool s_initialized = false;
static otto_servo_config_t s_config[OTTO_SERVO_COUNT];
static int s_last_angle[OTTO_SERVO_COUNT] = {0};

static uint32_t angle_to_duty(const otto_servo_config_t* cfg, int angle_deg) {
    int clamped = angle_deg;
    if (clamped < cfg->min_angle) {
        clamped = cfg->min_angle;
    } else if (clamped > cfg->max_angle) {
        clamped = cfg->max_angle;
    }

    if (cfg->invert) {
        clamped = cfg->max_angle - (clamped - cfg->min_angle);
    }

    const int angle_range = cfg->max_angle - cfg->min_angle;
    const int pulse_range = OTTO_SERVO_MAX_PULSE_US - OTTO_SERVO_MIN_PULSE_US;
    const int pulse_us = OTTO_SERVO_MIN_PULSE_US +
        ((clamped - cfg->min_angle) * pulse_range) / angle_range;

    const uint32_t max_duty = (1U << OTTO_SERVO_TIMER_RESOLUTION) - 1U;
    const uint32_t period_us = 1000000U / OTTO_SERVO_FREQ_HZ;
    return (pulse_us * max_duty) / period_us;
}

esp_err_t servo_control_init(const otto_servo_config_t config[OTTO_SERVO_COUNT]) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = OTTO_SERVO_TIMER_RESOLUTION,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = OTTO_SERVO_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_cfg), TAG, "timer init failed");

    for (int i = 0; i < OTTO_SERVO_COUNT; ++i) {
        if (config[i].gpio_num < 0) {
            return ESP_ERR_INVALID_ARG;
        }
        s_config[i] = config[i];

        ledc_channel_config_t channel_cfg = {
            .gpio_num = config[i].gpio_num,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = (ledc_channel_t)i,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_0,
            .duty = 0,
            .hpoint = 0,
            .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
            .flags = {
                .output_invert = 0,
            },
        };
        ESP_RETURN_ON_ERROR(ledc_channel_config(&channel_cfg), TAG, "channel %d init failed", i);

        int neutral = config[i].neutral_angle;
        if (neutral < config[i].min_angle) {
            neutral = config[i].min_angle;
        } else if (neutral > config[i].max_angle) {
            neutral = config[i].max_angle;
        }
        s_last_angle[i] = neutral;

        uint32_t duty = angle_to_duty(&s_config[i], neutral);
        ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)i, duty),
                            TAG, "set neutral duty failed for servo %d", i);
        ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)i),
                            TAG, "update neutral duty failed for servo %d", i);
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Servo control ready for %d servos", OTTO_SERVO_COUNT);
    return ESP_OK;
}

esp_err_t servo_set_angle(otto_servo_id_t servo, int angle_deg) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (servo < OTTO_SERVO_FRONT_LEFT || servo > OTTO_SERVO_TAIL) {
        return ESP_ERR_INVALID_ARG;
    }

    const otto_servo_config_t* cfg = &s_config[servo];
    uint32_t duty = angle_to_duty(cfg, angle_deg);
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)servo, duty),
                        TAG, "set duty failed");
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)servo),
                        TAG, "update duty failed");

    if (angle_deg < cfg->min_angle) {
        s_last_angle[servo] = cfg->min_angle;
    } else if (angle_deg > cfg->max_angle) {
        s_last_angle[servo] = cfg->max_angle;
    } else {
        s_last_angle[servo] = angle_deg;
    }
    return ESP_OK;
}

int servo_get_angle(otto_servo_id_t servo) {
    if (servo < OTTO_SERVO_FRONT_LEFT || servo > OTTO_SERVO_TAIL) {
        return 0;
    }
    return s_last_angle[servo];
}
