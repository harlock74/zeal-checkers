#pragma once

#include <stdint.h>
#include <zvb_gfx.h>

gfx_error load_checkers_palette(gfx_context* ctx);
gfx_error load_checkers_tileset(gfx_context* ctx);
uint8_t assets_get_layout_tile(uint8_t x, uint8_t y);
uint8_t assets_get_splash_bg_tile(uint8_t x, uint8_t y);
uint8_t assets_get_splash_text_tile(uint8_t x, uint8_t y);
