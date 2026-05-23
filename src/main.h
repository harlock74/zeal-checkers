#pragma once

#include <stdint.h>
#include <zvb_gfx.h>

#define SCREEN_TILE_W 40
#define SCREEN_TILE_H 30
#define TILE_PIXELS 16

#define LAYER_BOARD 0
#define LAYER_SPRITES 1

/* Runtime transparent tile injected after loading the tileset. */
#define EMPTY_TILE 255
#define SHARED_SCRATCH_BUF_SIZE 4096
/* Keep syscall scratch buffer inside one 16KB virtual page. */
#define G_BUF_ADDR 0xA000

#define FLAG_OFF 0
#define FLAG_ON 1
#define U8_MAX_VALUE 255

/* Checkers board dimensions expressed in 16x16 tiles. */
#define CHECKERS_BOARD_SQUARES 8
#define CHECKERS_SQUARE_TILE_W 3
#define CHECKERS_SQUARE_TILE_H 3
#define CHECKERS_BOARD_TILE_W (CHECKERS_BOARD_SQUARES * CHECKERS_SQUARE_TILE_W)
#define CHECKERS_BOARD_TILE_H (CHECKERS_BOARD_SQUARES * CHECKERS_SQUARE_TILE_H)
#define CHECKERS_PIECES_PER_SIDE 12
#define CHECKERS_TOTAL_PIECES (CHECKERS_PIECES_PER_SIDE * 2)
#define CHECKERS_PIECE_TILE_W 3
#define CHECKERS_PIECE_TILE_H 3
#define CHECKERS_PIECE_SPRITES (CHECKERS_PIECE_TILE_W * CHECKERS_PIECE_TILE_H)
#define CHECKERS_COMPUTER_REPLY_DELAY_FRAMES 30
#define CHECKERS_AI_EASY 0
#define CHECKERS_AI_MEDIUM 1
#define CHECKERS_AI_HARD 2
#define CHECKERS_AI_DIFFICULTY CHECKERS_AI_MEDIUM
#define SCREEN_PIXEL_W (SCREEN_TILE_W * TILE_PIXELS)
#define SCREEN_PIXEL_H (SCREEN_TILE_H * TILE_PIXELS)
#define MOUSE_Y_DIRECTION 1

/* Tiled tile IDs used directly by the renderer, matching zeal-solitaire. */
#define OPEN_HAND_SELECTOR_TILE 2
#define CLOSED_HAND_SELECTOR_TILE 3

extern gfx_context vctx;
extern uint8_t __at(G_BUF_ADDR) g_buf[SHARED_SCRATCH_BUF_SIZE];

void init(void);
void deinit(void);
void update(void);
void draw(void);
