#include "behavior.h"

#include "sdkconfig.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <freertos/queue.h>
#include <esp_check.h>
#include <esp_log.h>
#include <esp_random.h>
#include <string.h>

#define TAG "otto_behavior"

#ifndef CONFIG_OTTO_THINKING_TIME_MIN_MS
#define CONFIG_OTTO_THINKING_TIME_MIN_MS 700
#endif
#ifndef CONFIG_OTTO_THINKING_TIME_MAX_MS
#define CONFIG_OTTO_THINKING_TIME_MAX_MS 1800
#endif
#ifndef CONFIG_OTTO_IDLE_TIMEOUT_MS
#define CONFIG_OTTO_IDLE_TIMEOUT_MS 7000
#endif
#ifndef CONFIG_OTTO_ANIMATION_INTERVAL_MS
#define CONFIG_OTTO_ANIMATION_INTERVAL_MS 80
#endif

/** Servo smoothing: percent of remaining error closed each animation tick (not in Kconfig). */
#define OTTO_POSE_LERP_PERCENT 28

#define MOOD_STT_DELTA           6
#define MOOD_IDLE_PENALTY        4
#define MOOD_LLM_HAPPY_DELTA     5
#define MOOD_LLM_EXCITED_DELTA   8
#define MOOD_LLM_SAD_DELTA       (-12)

volatile otto_state_t g_otto_current_state = OTTO_STATE_IDLE;
volatile int g_mood_level = 0;

typedef enum {
    OTTO_FSM_EVT_STT = 0,
    OTTO_FSM_EVT_LLM,
    OTTO_FSM_EVT_THINKING_DONE,
    OTTO_FSM_EVT_IDLE_TIMEOUT,
    OTTO_FSM_EVT_SET_STATE,
} otto_fsm_evt_type_t;

typedef struct {
    otto_fsm_evt_type_t type;
    otto_state_t state;
    char llm_emotion[48];
} otto_fsm_msg_t;

static bool s_ready = false;
static TimerHandle_t s_idle_timer = NULL;
static TimerHandle_t s_thinking_timer = NULL;
static otto_state_t s_pending_emotion_state = OTTO_STATE_HAPPY;
static otto_emotion_callback_t s_emotion_cb = NULL;
static otto_reaction_callback_t s_reaction_cb = NULL;
static QueueHandle_t s_fsm_queue = NULL;
static TaskHandle_t s_fsm_task = NULL;

/** 0: user STT listening, 1: "curious" branch from neutral LLM mapping. */
static uint8_t s_listen_context = 0;

static int s_cur_fl = 90;
static int s_cur_fr = 90;
static int s_cur_rl = 90;
static int s_cur_rr = 90;
static int s_cur_tail = 90;

static int s_tgt_fl = 90;
static int s_tgt_fr = 90;
static int s_tgt_rl = 90;
static int s_tgt_rr = 90;
static int s_tgt_tail = 90;

static void apply_pose_from_current(void) {
    servo_set_angle(OTTO_SERVO_FRONT_LEFT, s_cur_fl);
    servo_set_angle(OTTO_SERVO_FRONT_RIGHT, s_cur_fr);
    servo_set_angle(OTTO_SERVO_REAR_LEFT, s_cur_rl);
    servo_set_angle(OTTO_SERVO_REAR_RIGHT, s_cur_rr);
    servo_set_angle(OTTO_SERVO_TAIL, s_cur_tail);
}

static void clamp_mood(void) {
    if (g_mood_level > 100) {
        g_mood_level = 100;
    } else if (g_mood_level < -100) {
        g_mood_level = -100;
    }
}

static uint32_t random_thinking_duration_ms(void) {
    int mn = CONFIG_OTTO_THINKING_TIME_MIN_MS;
    int mx = CONFIG_OTTO_THINKING_TIME_MAX_MS;
    if (mx < mn) {
        int t = mn;
        mn = mx;
        mx = t;
    }
    const uint32_t span = (uint32_t)(mx - mn) + 1U;
    return (uint32_t)mn + (esp_random() % span);
}

static otto_state_t resolve_llm_to_pending_state(const char* emotion) {
    const uint32_t r = esp_random() % 100U;
    const char* e = emotion;

    if (e == NULL || e[0] == '\0') {
        return (r < 70U) ? OTTO_STATE_HAPPY : OTTO_STATE_EXCITED;
    }

    if (strcmp(e, "neutral") == 0 || strcmp(e, "calm") == 0) {
        return (r < 55U) ? OTTO_STATE_IDLE : OTTO_STATE_LISTENING;
    }
    if (strcmp(e, "happy") == 0 || strcmp(e, "joy") == 0) {
        return (r < 70U) ? OTTO_STATE_HAPPY : OTTO_STATE_EXCITED;
    }
    if (strcmp(e, "sad") == 0 || strcmp(e, "angry") == 0 || strcmp(e, "cry") == 0) {
        return (r < 85U) ? OTTO_STATE_SAD : OTTO_STATE_HAPPY;
    }
    if (strcmp(e, "excited") == 0 || strcmp(e, "surprised") == 0) {
        return (r < 80U) ? OTTO_STATE_EXCITED : OTTO_STATE_HAPPY;
    }
    return (r < 65U) ? OTTO_STATE_HAPPY : OTTO_STATE_EXCITED;
}

static int mood_amplitude(void) {
    int m = g_mood_level;
    if (m < 0) {
        m = -m;
    }
    return 8 + (m * 12) / 100;
}

static void compute_ideal_targets(otto_state_t st, uint32_t step, int out[5]) {
    const int ma = mood_amplitude();
    const uint32_t p = step;

    switch (st) {
        case OTTO_STATE_IDLE:
            out[0] = 90;
            out[1] = 90;
            out[2] = 90;
            out[3] = 90;
            out[4] = 90 + (g_mood_level * 5) / 100;
            break;
        case OTTO_STATE_LISTENING: {
            int wag = ((p / 4U) % 2U) ? (8 + ma / 4) : -(8 + ma / 4);
            out[0] = 90;
            out[1] = 90;
            out[2] = 90;
            out[3] = 90;
            out[4] = 90 + wag;
            break;
        }
        case OTTO_STATE_THINKING: {
            const uint32_t ease = (p < 14U) ? p : 14U;
            const int tilt_l = 90 + ((-6) * (int)ease) / 14;
            const int tilt_r = 90 + (6 * (int)ease) / 14;
            const int wobble = (int)((p % 18U) - 9) * 2;
            out[0] = tilt_l + wobble / 3;
            out[1] = tilt_r - wobble / 3;
            out[2] = 90 + wobble / 4;
            out[3] = 90 - wobble / 4;
            out[4] = 88 + ((p / 6U) % 2U ? 6 : -6);
            break;
        }
        case OTTO_STATE_HAPPY: {
            int amp = 18 + ma / 2;
            out[0] = 90 + (((p / 3U) % 2U) ? amp : -amp);
            out[1] = 90 + (((p / 3U) % 2U) ? -amp : amp);
            out[2] = 90 + (((p / 3U) % 2U) ? amp : -amp);
            out[3] = 90 + (((p / 3U) % 2U) ? -amp : amp);
            out[4] = 90 + (((p / 2U) % 2U) ? (12 + ma / 3) : -(12 + ma / 3));
            break;
        }
        case OTTO_STATE_SAD:
            out[0] = 85 + (g_mood_level / 25);
            out[1] = 85 - (g_mood_level / 25);
            out[2] = 95;
            out[3] = 95;
            out[4] = 58;
            break;
        case OTTO_STATE_EXCITED: {
            int eamp = 22 + ma;
            out[0] = 90 + (((p / 2U) % 2U) ? eamp : -eamp);
            out[1] = 90 + (((p / 2U) % 2U) ? -eamp : eamp);
            out[2] = 90 + (((p / 2U) % 2U) ? (eamp * 3) / 4 : -(eamp * 3) / 4);
            out[3] = 90 + (((p / 2U) % 2U) ? -(eamp * 3) / 4 : (eamp * 3) / 4);
            out[4] = 90 + (((p % 2U) == 0U) ? (18 + ma / 2) : -(18 + ma / 2));
            break;
        }
        default:
            out[0] = out[1] = out[2] = out[3] = out[4] = 90;
            break;
    }
}

static void copy_targets_to_s_tgt(const int ideal[5]) {
    s_tgt_fl = ideal[0];
    s_tgt_fr = ideal[1];
    s_tgt_rl = ideal[2];
    s_tgt_rr = ideal[3];
    s_tgt_tail = ideal[4];
}

static void lerp_current_toward_targets(void) {
    const int k = OTTO_POSE_LERP_PERCENT;
    int* curv[] = {&s_cur_fl, &s_cur_fr, &s_cur_rl, &s_cur_rr, &s_cur_tail};
    int tgtv[] = {s_tgt_fl, s_tgt_fr, s_tgt_rl, s_tgt_rr, s_tgt_tail};
    for (int i = 0; i < 5; ++i) {
        int* c = curv[i];
        const int t = tgtv[i];
        int diff = t - *c;
        if (diff > -2 && diff < 2) {
            *c = t;
        } else {
            *c += (diff * k) / 100;
        }
    }
}

/**
 * Single place to sync UI (emotion/reaction) with FSM state and refresh motion targets.
 * Called only from the FSM task.
 */
static void apply_state_behavior(otto_state_t prev, otto_state_t next) {
    (void)prev;
    g_otto_current_state = next;

    if (s_emotion_cb != NULL) {
        switch (next) {
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
        switch (next) {
            case OTTO_STATE_LISTENING:
                s_reaction_cb(s_listen_context ? "curious" : "listening");
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

    int ideal[5];
    compute_ideal_targets(next, 0, ideal);
    copy_targets_to_s_tgt(ideal);

    if (s_idle_timer != NULL) {
        xTimerStop(s_idle_timer, 0);
        if (next != OTTO_STATE_IDLE) {
            xTimerChangePeriod(s_idle_timer, pdMS_TO_TICKS(CONFIG_OTTO_IDLE_TIMEOUT_MS), 0);
            xTimerStart(s_idle_timer, 0);
        }
    }
}

static void transition_to(otto_state_t next) {
    const otto_state_t prev = g_otto_current_state;
    if (next == prev) {
        return;
    }
    apply_state_behavior(prev, next);
}

/** Enter THINKING after an LLM hint; if already thinking, only reset micro-motion and timer. */
static void enter_thinking_from_llm(const char* emotion) {
    s_pending_emotion_state = resolve_llm_to_pending_state(emotion);
    s_listen_context = 0;

    const otto_state_t prev = g_otto_current_state;
    if (prev != OTTO_STATE_THINKING) {
        apply_state_behavior(prev, OTTO_STATE_THINKING);
    } else {
        int ideal[5];
        compute_ideal_targets(OTTO_STATE_THINKING, 0, ideal);
        copy_targets_to_s_tgt(ideal);
        if (s_idle_timer != NULL) {
            xTimerStop(s_idle_timer, 0);
            xTimerChangePeriod(s_idle_timer, pdMS_TO_TICKS(CONFIG_OTTO_IDLE_TIMEOUT_MS), 0);
            xTimerStart(s_idle_timer, 0);
        }
    }

    const uint32_t ms = random_thinking_duration_ms();
    if (s_thinking_timer != NULL) {
        xTimerStop(s_thinking_timer, 0);
        xTimerChangePeriod(s_thinking_timer, pdMS_TO_TICKS(ms), 0);
        xTimerStart(s_thinking_timer, 0);
    }
}

static void transition_or_refresh_stt(void) {
    g_mood_level += MOOD_STT_DELTA;
    clamp_mood();
    s_listen_context = 0;
    if (g_otto_current_state == OTTO_STATE_LISTENING) {
        if (s_idle_timer != NULL) {
            xTimerStop(s_idle_timer, 0);
            xTimerChangePeriod(s_idle_timer, pdMS_TO_TICKS(CONFIG_OTTO_IDLE_TIMEOUT_MS), 0);
            xTimerStart(s_idle_timer, 0);
        }
        return;
    }
    transition_to(OTTO_STATE_LISTENING);
}

static void on_thinking_done(void) {
    if (g_otto_current_state != OTTO_STATE_THINKING) {
        return;
    }
    const otto_state_t pending = s_pending_emotion_state;
    if (pending == OTTO_STATE_HAPPY) {
        g_mood_level += MOOD_LLM_HAPPY_DELTA;
    } else if (pending == OTTO_STATE_EXCITED) {
        g_mood_level += MOOD_LLM_EXCITED_DELTA;
    } else if (pending == OTTO_STATE_SAD) {
        g_mood_level += MOOD_LLM_SAD_DELTA;
    }
    clamp_mood();

    if (pending == OTTO_STATE_LISTENING) {
        s_listen_context = 1;
    } else {
        s_listen_context = 0;
    }
    transition_to(pending);
}

static void on_idle_timeout(void) {
    g_mood_level -= MOOD_IDLE_PENALTY;
    clamp_mood();
    transition_to(OTTO_STATE_IDLE);
}

static void on_llm_message(const char* emotion) {
    enter_thinking_from_llm(emotion);
}

static void fsm_task(void* arg) {
    (void)arg;
    otto_fsm_msg_t msg;
    for (;;) {
        if (xQueueReceive(s_fsm_queue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        switch (msg.type) {
            case OTTO_FSM_EVT_STT:
                if (s_thinking_timer != NULL) {
                    xTimerStop(s_thinking_timer, 0);
                }
                transition_or_refresh_stt();
                break;
            case OTTO_FSM_EVT_LLM:
                if (s_thinking_timer != NULL) {
                    xTimerStop(s_thinking_timer, 0);
                }
                on_llm_message(msg.llm_emotion);
                break;
            case OTTO_FSM_EVT_THINKING_DONE:
                on_thinking_done();
                break;
            case OTTO_FSM_EVT_IDLE_TIMEOUT:
                on_idle_timeout();
                break;
            case OTTO_FSM_EVT_SET_STATE:
                if (s_thinking_timer != NULL) {
                    xTimerStop(s_thinking_timer, 0);
                }
                apply_state_behavior((otto_state_t)g_otto_current_state, msg.state);
                break;
            default:
                break;
        }
    }
}

static void idle_timer_callback(TimerHandle_t timer) {
    (void)timer;
    const otto_fsm_msg_t msg = {.type = OTTO_FSM_EVT_IDLE_TIMEOUT};
    if (s_fsm_queue != NULL) {
        xQueueSend(s_fsm_queue, &msg, 0);
    }
}

static void thinking_timer_callback(TimerHandle_t timer) {
    (void)timer;
    const otto_fsm_msg_t msg = {.type = OTTO_FSM_EVT_THINKING_DONE};
    if (s_fsm_queue != NULL) {
        xQueueSend(s_fsm_queue, &msg, 0);
    }
}

static bool post_message(const otto_fsm_msg_t* msg, TickType_t ticks_to_wait) {
    if (s_fsm_queue == NULL) {
        return false;
    }
    if (xQueueSend(s_fsm_queue, msg, ticks_to_wait) != pdTRUE) {
        ESP_LOGW(TAG, "FSM queue full, event dropped");
        return false;
    }
    return true;
}

static void animation_task(void* arg) {
    (void)arg;
    const TickType_t period = pdMS_TO_TICKS(CONFIG_OTTO_ANIMATION_INTERVAL_MS);
    otto_state_t last_st = OTTO_STATE_IDLE;
    uint32_t anim_step = 0;
    for (;;) {
        const otto_state_t st = g_otto_current_state;
        if (st != last_st) {
            last_st = st;
            anim_step = 0;
        } else {
            anim_step++;
        }
        int ideal[5];
        compute_ideal_targets(st, anim_step, ideal);
        copy_targets_to_s_tgt(ideal);
        lerp_current_toward_targets();
        apply_pose_from_current();
        vTaskDelay(period);
    }
}

esp_err_t behavior_init(const otto_servo_config_t config[OTTO_SERVO_COUNT]) {
    if (s_ready) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(servo_control_init(config), TAG, "servo init failed");

    s_fsm_queue = xQueueCreate(12, sizeof(otto_fsm_msg_t));
    if (s_fsm_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    s_idle_timer = xTimerCreate("otto_idle", pdMS_TO_TICKS(CONFIG_OTTO_IDLE_TIMEOUT_MS), pdFALSE, NULL,
                                idle_timer_callback);
    s_thinking_timer =
        xTimerCreate("otto_think", pdMS_TO_TICKS(1000), pdFALSE, NULL, thinking_timer_callback);
    if (s_idle_timer == NULL || s_thinking_timer == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(fsm_task, "otto_fsm", 4096, NULL, 5, &s_fsm_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(animation_task, "otto_anim", 4096, NULL, 4, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    s_cur_fl = s_cur_fr = s_cur_rl = s_cur_rr = s_cur_tail = 90;
    s_tgt_fl = s_tgt_fr = s_tgt_rl = s_tgt_rr = s_tgt_tail = 90;
    g_otto_current_state = OTTO_STATE_IDLE;
    g_mood_level = 0;

    s_ready = true;
    ESP_LOGI(TAG, "Otto FSM initialized (thinking %d-%d ms, idle %d ms, anim %d ms, lerp %d%%)",
             CONFIG_OTTO_THINKING_TIME_MIN_MS, CONFIG_OTTO_THINKING_TIME_MAX_MS,
             CONFIG_OTTO_IDLE_TIMEOUT_MS, CONFIG_OTTO_ANIMATION_INTERVAL_MS, OTTO_POSE_LERP_PERCENT);
    return ESP_OK;
}

void behavior_set_callbacks(otto_emotion_callback_t emotion_cb, otto_reaction_callback_t reaction_cb) {
    s_emotion_cb = emotion_cb;
    s_reaction_cb = reaction_cb;
}

void set_state(otto_state_t new_state) {
    if (!s_ready) {
        return;
    }
    const otto_fsm_msg_t msg = {.type = OTTO_FSM_EVT_SET_STATE, .state = new_state, .llm_emotion = {0}};
    (void)post_message(&msg, pdMS_TO_TICKS(50));
}

otto_state_t get_state(void) {
    return g_otto_current_state;
}

int otto_get_mood_level(void) {
    return g_mood_level;
}

void otto_on_stt_event(void) {
    if (!s_ready) {
        return;
    }
    const otto_fsm_msg_t msg = {.type = OTTO_FSM_EVT_STT, .state = OTTO_STATE_IDLE, .llm_emotion = {0}};
    (void)post_message(&msg, pdMS_TO_TICKS(50));
}

void otto_on_llm_event(const char* llm_emotion) {
    if (!s_ready) {
        return;
    }
    otto_fsm_msg_t msg = {.type = OTTO_FSM_EVT_LLM, .state = OTTO_STATE_IDLE, .llm_emotion = {0}};
    if (llm_emotion != NULL) {
        strncpy(msg.llm_emotion, llm_emotion, sizeof(msg.llm_emotion) - 1);
        msg.llm_emotion[sizeof(msg.llm_emotion) - 1] = '\0';
    }
    (void)post_message(&msg, pdMS_TO_TICKS(50));
}

void mover_cola(void) {
    set_state(OTTO_STATE_HAPPY);
}

void caminar_adelante(void) {
    set_state(OTTO_STATE_EXCITED);
}
