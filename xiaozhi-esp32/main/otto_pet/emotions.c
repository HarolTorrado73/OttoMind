#include "emotions.h"
#include "behavior.h"

void emocion_feliz(void) {
    set_state(OTTO_STATE_HAPPY);
}

void emocion_triste(void) {
    set_state(OTTO_STATE_SAD);
}
