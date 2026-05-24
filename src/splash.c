#include "splash.h"

#include "main.h"
#include "render.h"

void splash_show(void)
{
    render_clear_drag_piece();
    render_splash();
}

uint8_t splash_select_difficulty(const KeyEvents* ev)
{
    if (ev->down) {
        return CHECKERS_AI_EASY;
    }
    if (ev->right) {
        return CHECKERS_AI_MEDIUM;
    }
    if (ev->up) {
        return CHECKERS_AI_HARD;
    }

    return U8_MAX_VALUE;
}
