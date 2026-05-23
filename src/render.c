#include <stdint.h>

#include <zvb_gfx.h>
#include <zvb_sprite.h>
#include <zgdk.h>

#include "main.h"
#include "assets.h"
#include "render.h"

#define SELECTOR_SPRITE_COUNT 1
/* ZGDK's sprite arena keeps one guard slot. */
#define SPRITE_ARENA_CAPACITY (SELECTOR_SPRITE_COUNT + 1)

/* Top-left Tiled tile IDs for each 3x3 piece block in assets/checkers.gif. */
#define LIGHT_MAN_TILE 48
#define DARK_MAN_TILE 52
#define LIGHT_KING_TILE 56
#define DARK_KING_TILE 60
#define TILESET_ROW_TILE_COUNT 16
#define SELECTOR_CENTER_TILE_OFFSET 1

static gfx_sprite sprite_arena[SPRITE_ARENA_CAPACITY];
static gfx_sprite* selector_sprites[SELECTOR_SPRITE_COUNT];

static uint8_t tile_at_offset(uint8_t base_tile, uint8_t col, uint8_t row)
{
    return (uint8_t)(base_tile + col + (row * TILESET_ROW_TILE_COUNT));
}

static uint16_t sprite_pos_from_tile(uint8_t tile)
{
    return (uint16_t)(((uint16_t)tile * TILE_PIXELS) + TILE_PIXELS);
}

static uint16_t sprite_pos_from_pixel(uint16_t pixel)
{
    return (uint16_t)(pixel + TILE_PIXELS);
}

static uint8_t piece_tile_from_board_coord(uint8_t board_coord)
{
    return (uint8_t)(board_coord * CHECKERS_SQUARE_TILE_W);
}

void render_board(void)
{
    /*
     * Milestone 1:
     * draw the static Checkers tilemap exactly as exported from Tiled.
     */
    for (uint8_t y = 0; y < SCREEN_TILE_H; y++) {
        for (uint8_t x = 0; x < SCREEN_TILE_W; x++) {
            gfx_tilemap_place(&vctx, assets_get_layout_tile(x, y), LAYER_BOARD, x, y);
            gfx_tilemap_place(&vctx, EMPTY_TILE, LAYER_SPRITES, x, y);
        }
    }
}

void render_clear_sprite_layer(void)
{
    for (uint8_t y = 0; y < SCREEN_TILE_H; y++) {
        for (uint8_t x = 0; x < SCREEN_TILE_W; x++) {
            gfx_tilemap_place(&vctx, EMPTY_TILE, LAYER_SPRITES, x, y);
        }
    }
}

void render_clear_piece_square(uint8_t board_x, uint8_t board_y)
{
    uint8_t tile_x = piece_tile_from_board_coord(board_x);
    uint8_t tile_y = piece_tile_from_board_coord(board_y);

    for (uint8_t row = 0; row < CHECKERS_PIECE_TILE_H; row++) {
        for (uint8_t col = 0; col < CHECKERS_PIECE_TILE_W; col++) {
            gfx_tilemap_place(
                &vctx,
                EMPTY_TILE,
                LAYER_SPRITES,
                (uint8_t)(tile_x + col),
                (uint8_t)(tile_y + row));
        }
    }
}

gfx_error render_init_sprites(void)
{
    gfx_sprite sprite = {
        .y = TILE_PIXELS,
        .x = TILE_PIXELS,
        .tile = EMPTY_TILE,
        .flags = SPRITE_NONE,
        .options = SPRITE_OPTION_NONE,
    };

    /*
     * Static pieces are drawn on layer 1 as 3x3 tile blocks. Hardware sprites
     * are reserved for moving overlays such as the selector.
     */
    if (sprites_register_arena(sprite_arena, SPRITE_ARENA_CAPACITY) != GFX_SUCCESS) {
        return GFX_FAILURE;
    }

    for (uint8_t i = 0; i < SELECTOR_SPRITE_COUNT; i++) {
        selector_sprites[i] = sprites_register(sprite);
        if (selector_sprites[i] == NULL) {
            return GFX_FAILURE;
        }
    }

    return GFX_SUCCESS;
}

void render_set_selector(uint8_t board_x, uint8_t board_y, uint8_t tile, uint8_t visible)
{
    uint8_t tile_x = piece_tile_from_board_coord(board_x);
    uint8_t tile_y = piece_tile_from_board_coord(board_y);
    gfx_sprite* sprite = selector_sprites[0];

    sprite->tile = visible ? tile : EMPTY_TILE;
    sprite->x = sprite_pos_from_tile((uint8_t)(tile_x + SELECTOR_CENTER_TILE_OFFSET));
    sprite->y = sprite_pos_from_tile((uint8_t)(tile_y + SELECTOR_CENTER_TILE_OFFSET));
}

void render_set_selector_pixel(uint16_t pixel_x, uint16_t pixel_y, uint8_t tile, uint8_t visible)
{
    gfx_sprite* sprite = selector_sprites[0];

    sprite->tile = visible ? tile : EMPTY_TILE;
    sprite->x = sprite_pos_from_pixel(pixel_x);
    sprite->y = sprite_pos_from_pixel(pixel_y);
}

void render_set_piece(uint8_t piece_index, uint8_t board_x, uint8_t board_y, uint8_t player_piece, uint8_t king)
{
    uint8_t base_tile;
    uint8_t tile_x;
    uint8_t tile_y;

    if (piece_index >= CHECKERS_TOTAL_PIECES) {
        return;
    }

    if (player_piece) {
        base_tile = king ? DARK_KING_TILE : DARK_MAN_TILE;
    } else {
        base_tile = king ? LIGHT_KING_TILE : LIGHT_MAN_TILE;
    }

    tile_x = piece_tile_from_board_coord(board_x);
    tile_y = piece_tile_from_board_coord(board_y);

    for (uint8_t row = 0; row < CHECKERS_PIECE_TILE_H; row++) {
        for (uint8_t col = 0; col < CHECKERS_PIECE_TILE_W; col++) {
            gfx_tilemap_place(
                &vctx,
                tile_at_offset(base_tile, col, row),
                LAYER_SPRITES,
                (uint8_t)(tile_x + col),
                (uint8_t)(tile_y + row));
        }
    }
}

void render_draw_sprites(void)
{
    sprites_render(&vctx);
}
