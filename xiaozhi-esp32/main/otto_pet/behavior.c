#include "behavior.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <esp_check.h>
#include <esp_log.h>
#include <string.h>

#define TAG "otto_behavior"

volatile otto_state_t g_otto_current_state = OTTO_STATE_IDLE;

static bool s_ready = false;
static uint32_t s_anim_step = 0;
static TimerHandle_t s_idle_timer = NULL;
static TimerHandle_t s_thinking_timer = NULL;
static otto_state_t s_pending_emotion_state = OTTO_STATE_HAPPY;
static otto_emotion_callback_t s_emotion_cb = NULL;
static otto_reaction_callback_t s_reaction_cb = NULL;

static otto_state_t map_llm_emotion(const char* emotion) {
    if (emotion == NULL) {
        return OTTO_STATE_HAPPY;
    }
    if (strcmp(emotion, "sad") == 0 || strcmp(emotion, "angry") == 0 ||
        strcmp(emotion, "cry") == 0) {
        return OTTO_STATE_SAD;
    }
    if (strcmp(emotion, "excited") == 0 || strcmp(emotion, "surprised") == 0) {
        return OTTO_STATE_EXCITED;
    }
    return OTTO_STATE_HAPPY;
}

static void apply_pose(int fl, int fr, int rl, int rr, int tail) {
    servo_set_angle(OTTO_SERVO_FRONT_LEFT, fl);
    servo_set_angle(OTTO_SERVO_FRONT_RIGHT, fr);
    servo_set_angle(OTTO_SERVO_REAR_LEFT, rl);
    servo_set_angle(OTTO_SERVO_REAR_RIGHT, rr);
    servo_set_angle(OTTO_SERVO_TAIL, tail);
}

static void on_state_enter(otto_state_t state) {
    if (s_emotion_cb != NULL) {
        switch (state) {
            case OTTO_STATE_IDLE:
                s_emotion_cb("neutral");
                break;
            case OTTO_STATE_LISTENING:
                s_emotion_cb("neutral");
                break;
            case OTTO_STATE_THINKING:
                s_emotion_cb("thinking");
                break;
            case OTTO_STATE_HAPPY:
                s_emotion_cb("happy");
                break;
            case OTTO_STATE_SAD:
                s_emotion_cb("sad");
                break;
            case OTTO_STATE_EXCITED:
                s_emotion_cb("excited");
                break;
            default:
                break;
        }
    }

    if (s_reaction_cb != NULL) {
        switch (state) {
            case OTTO_STATE_LISTENING:
                s_reaction_cb("listening");
                break;
            case OTTO_STATE_THINKING:
                s_reaction_cb("thinking");
                break;
            case OTTO_STATE_HAPPY:
                s_reaction_cb("happy");
                break;
            case OTTO_STATE_SAD:
                s_reaction_cb("sad");
                break;
            case OTTO_STATE_EXCITED:
                s_reaction_cb("excited");
                break;
            default:
                break;
        }
    }
}

static void schedule_idle_timeout(void) {
    if (s_idle_timer == NULL) {
        return;
    }
    xTimerStop(s_idle_timer, 0);
    if (g_otto_current_state != OTTO_STATE_IDLE) {
        xTimerChangePeriod(s_idle_timer, pdMS_TO_TICKS(7000), 0);
        xTimerStart(s_idle_timer, 0);
    }
}

static void animation_task(void* arg) {
    (void)arg;
    while (true) {
        const uint32_t p = s_anim_step++;
        switch (g_otto_current_state) {
            case OTTO_STATE_IDLE:
                apply_pose(90, 90, 90, 90, 90);
                break;
            case OTTO_STATE_LISTENING:
                apply_pose(90, 90, 90, 90, (p / 4) % 2 ? 102 : 78);
                break;
            case OTTO_STATE_THINKING:
                apply_pose((p / 8) % 2 ? 95 : 85, (p / 8) % 2 ? 85 : 95, 90, 90,
                           (p / 10) % 2 ? 98 : 82);
                break;
            case OTTO_STATE_HAPPY:
                apply_pose((p / 3) % 2 ? 108 : 72, (p / 3) % 2 ? 72 : 108,
                           (p / 3) % 2 ? 108 : 72, (p / 3) % 2 ? 72 : 108,
                           (p / 2) % 2 ? 120 : 60);
                break;
            case OTTO_STATE_SAD:
                apply_pose(85, 85, 95, 95, 60);
                break;
            case OTTO_STATE_EXCITED:
                apply_pose((p / 2) % 2 ? 120 : 60, (p / 2) % 2 ? 60 : 120,
                           (p / 2) % 2 ? 110 : 70, (p / 2) % 2 ? 70 : 110,
                           (p % 2) ? 130 : 50);
                break;
            default:
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(80));
    }
}

static void idle_timer_callback(TimerHandle_t timer) {
    (void)timer;
    set_state(OTTO_STATE_IDLE);
}

static void thinking_timer_callback(TimerHandle_t timer) {
    (void)timer;
    set_state(s_pending_emotion_state);
}

esp_err_t behavior_init(const otto_servo_config_t config[OTTO_SERVO_COUNT]) {
    if (s_ready) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(servo_control_init(config), TAG, "servo init failed");
    s_idle_timer = xTimerCreate("otto_idle", pdMS_TO_TICKS(7000), pdFALSE, NULL, idle_timer_callback);
    s_thinking_timer = xTimerCreate("otto_thinking", pdMS_TO_TICKS(800), pdFALSE, NULL, thinking_timer_callback);
    if (s_idle_timer == NULL || s_thinking_timer == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(animation_task, "otto_anim", 3072, NULL, 4, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    s_ready = true;
    g_otto_current_state = OTTO_STATE_IDLE;
    ESP_LOGI(TAG, "Behavior module initialized");
    return ESP_OK;
}

void behavior_set_callbacks(otto_emotion_callback_t emotion_cb, otto_reaction_callback_t reaction_cb) {
    s_emotion_cb = emotion_cb;
    s_reaction_cb = reaction_cb;
}

void set_state(otto_state_t new_state) {
    if (!s_ready || new_state == g_otto_current_state) {
        return;
    }
    g_otto_current_state = new_state;
    s_anim_step = 0;
    on_state_enter(new_state);
    schedule_idle_timeout();
}

otto_state_t get_state(void) {
    return g_otto_current_state;
}

void otto_on_stt_event(void) {
    if (!s_ready) {
        return;
    }
    xTimerStop(s_thinking_timer, 0);
    set_state(OTTO_STATE_LISTENING);
}

void otto_on_llm_event(const char* llm_emotion) {
    if (!s_ready) {
        return;
    }
    s_pending_emotion_state = map_llm_emotion(llm_emotion);
    set_state(OTTO_STATE_THINKING);
    xTimerChangePeriod(s_thinking_timer, pdMS_TO_TICKS(900), 0);
    xTimerStart(s_thinking_timer, 0);
}

void mover_cola(void) {
    set_state(OTTO_STATE_HAPPY);
}

void caminar_adelante(void) {
    set_state(OTTO_STATE_EXCITED);
}
