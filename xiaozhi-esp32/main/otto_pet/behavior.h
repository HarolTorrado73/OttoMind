#ifndef OTTO_PET_BEHAVIOR_H
#define OTTO_PET_BEHAVIOR_H

#include <esp_err.h>
#include "servo_control.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OTTO_STATE_IDLE = 0,
    OTTO_STATE_LISTENING,
    OTTO_STATE_THINKING,
    OTTO_STATE_HAPPY,
    OTTO_STATE_SAD,
    OTTO_STATE_EXCITED,
} otto_state_t;

extern volatile otto_state_t g_otto_current_state;

typedef void (*otto_emotion_callback_t)(const char* emotion);
typedef void (*otto_reaction_callback_t)(const char* reaction);

esp_err_t behavior_init(const otto_servo_config_t config[OTTO_SERVO_COUNT]);
void behavior_set_callbacks(otto_emotion_callback_t emotion_cb, otto_reaction_callback_t reaction_cb);
void set_state(otto_state_t new_state);
otto_state_t get_state(void);
void otto_on_stt_event(void);
void otto_on_llm_event(const char* llm_emotion);

void mover_cola(void);
void caminar_adelante(void);

#ifdef __cplusplus
}
#endif

#endif  // OTTO_PET_BEHAVIOR_H
