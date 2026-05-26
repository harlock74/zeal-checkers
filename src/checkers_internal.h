#pragma once

#include <stdint.h>

#include "main.h"

#define COMPUTER_START_ROW_COUNT 3
#define PLAYER_START_ROW_FIRST 5
#define PIECE_IS_COMPUTER FLAG_OFF
#define PIECE_IS_PLAYER FLAG_ON
#define PIECE_IS_MAN FLAG_OFF
#define PIECE_IS_KING FLAG_ON
#define PIECE_EMPTY 0
#define PIECE_COMPUTER_MAN 1
#define PIECE_PLAYER_MAN 2
#define PIECE_COMPUTER_KING 3
#define PIECE_PLAYER_KING 4
#define PLAYER_PROMOTION_ROW 0
#define COMPUTER_PROMOTION_ROW (CHECKERS_BOARD_SQUARES - 1)
#define COMPUTER_BACK_ROW 0
#define SELECTOR_START_X 1
#define SELECTOR_START_Y PLAYER_START_ROW_FIRST
#define SELECTOR_MIN 0
#define SELECTOR_MAX (CHECKERS_BOARD_SQUARES - 1)
#define NO_SELECTION U8_MAX_VALUE
#define PLAYER_MOVE_DY -1
#define COMPUTER_MOVE_DY 1
#define MOVE_DISTANCE_ONE 1
#define MOVE_DISTANCE_CAPTURE 2
#define MOVE_NONE 0
#define MOVE_SIMPLE 1
#define MOVE_CAPTURE 2
#define TURN_PLAYER FLAG_ON
#define TURN_COMPUTER FLAG_OFF
#define GAME_PLAYING 0
#define GAME_PLAYER_WIN 1
#define GAME_COMPUTER_WIN 2
#define TILE_CENTER_OFFSET 8

typedef struct {
    uint8_t from_x;
    uint8_t from_y;
    uint8_t to_x;
    uint8_t to_y;
    uint8_t piece;
    uint8_t move_type;
    uint8_t score;
} CandidateMove;

extern uint8_t board[CHECKERS_BOARD_SQUARES][CHECKERS_BOARD_SQUARES];

uint8_t coord_on_board(int8_t coord);
uint8_t move_type_for_piece(uint8_t from_x, uint8_t from_y, uint8_t to_x, uint8_t to_y, uint8_t piece);
uint8_t piece_has_capture(uint8_t from_x, uint8_t from_y);
uint8_t piece_is_player(uint8_t piece);
uint8_t piece_is_computer(uint8_t piece);
uint8_t piece_is_king(uint8_t piece);
uint8_t piece_matches_turn(uint8_t piece, uint8_t turn);
uint8_t piece_promotes_at(uint8_t piece, uint8_t y);
