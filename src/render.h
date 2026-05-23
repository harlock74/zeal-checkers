#pragma once

#include <stdint.h>
#include <zvb_gfx.h>

gfx_error render_init_sprites(void);
void render_board(void);
void render_clear_sprite_layer(void);
void render_clear_piece_square(uint8_t board_x, uint8_t board_y);
void render_set_selector(uint8_t board_x, uint8_t board_y, uint8_t tile, uint8_t visible);
void render_set_selector_pixel(uint16_t pixel_x, uint16_t pixel_y, uint8_t tile, uint8_t visible);
void render_set_piece(uint8_t piece_index, uint8_t board_x, uint8_t board_y, uint8_t player_piece, uint8_t king);
void render_draw_sprites(void);
