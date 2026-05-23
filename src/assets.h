#pragma once

#include <stdint.h>
#include <zvb_gfx.h>

gfx_error load_checkers_palette(gfx_context* ctx);
gfx_error load_checkers_tileset(gfx_context* ctx);
uint8_t assets_get_layout_tile(uint8_t x, uint8_t y);
