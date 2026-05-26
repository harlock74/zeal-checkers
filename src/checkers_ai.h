#pragma once

#include <stdint.h>

#include "checkers_internal.h"

void checkers_ai_set_difficulty(uint8_t difficulty);
uint8_t checkers_ai_choose_computer_move(CandidateMove* move, uint8_t required_move_type);
uint8_t checkers_ai_choose_piece_capture(CandidateMove* move, uint8_t from_x, uint8_t from_y);
