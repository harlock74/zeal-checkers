#pragma once

#include <stdint.h>

#include "input.h"

void checkers_init_game(void);
void checkers_set_ai_difficulty(uint8_t difficulty);
void checkers_handle_input(const KeyEvents* ev);
