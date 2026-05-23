#include <stdint.h>

#include <zos_vfs.h>
#include <zvb_gfx.h>

#include "main.h"
#include "assets.h"

extern uint8_t _ztm_checkers_start;
extern uint8_t _ztm_checkers_end;

#define CHECKERS_TILESET_VRAM_SCALE 2

static gfx_error load_palette_from_file(gfx_context* ctx, const char* path)
{
    uint8_t from_color = 0;
    zos_dev_t dev = open(path, O_RDONLY);

    if (dev < 0) {
        return GFX_FAILURE;
    }

    while (1) {
        uint16_t size = sizeof(g_buf);

        if (read(dev, g_buf, &size) != ERR_SUCCESS) {
            close(dev);
            return GFX_FAILURE;
        }
        if (size == 0) {
            break;
        }

        if (gfx_palette_load(ctx, g_buf, size, from_color) != GFX_SUCCESS) {
            close(dev);
            return GFX_FAILURE;
        }

        from_color = (uint8_t)(from_color + (size / 2));
    }

    close(dev);
    return GFX_SUCCESS;
}

static gfx_error load_tileset_from_file(gfx_context* ctx, const char* path, uint8_t compression_mode, uint16_t vram_scale)
{
    uint16_t from_byte = 0;
    zos_dev_t dev = open(path, O_RDONLY);

    if (dev < 0) {
        return GFX_FAILURE;
    }

    while (1) {
        uint16_t size = sizeof(g_buf);
        gfx_tileset_options options = {
            .compression = compression_mode,
            .from_byte = from_byte,
            .pal_offset = 0,
            .opacity = 0,
        };

        if (read(dev, g_buf, &size) != ERR_SUCCESS) {
            close(dev);
            return GFX_FAILURE;
        }
        if (size == 0) {
            break;
        }

        if (gfx_tileset_load(ctx, g_buf, size, &options) != GFX_SUCCESS) {
            close(dev);
            return GFX_FAILURE;
        }

        from_byte = (uint16_t)(from_byte + (size * vram_scale));
    }

    close(dev);
    return GFX_SUCCESS;
}

gfx_error load_checkers_palette(gfx_context* ctx)
{
    return load_palette_from_file(ctx, "assets/checkers.ztp");
}

gfx_error load_checkers_tileset(gfx_context* ctx)
{
    if (load_tileset_from_file(ctx, "assets/checkers.zts", TILESET_COMP_4BIT, CHECKERS_TILESET_VRAM_SCALE) != GFX_SUCCESS) {
        return GFX_FAILURE;
    }

    /* Keep one explicit transparent tile reserved for clearing layer 1. */
    if (gfx_tileset_add_color_tile(ctx, U8_MAX_VALUE, FLAG_OFF) != GFX_SUCCESS) {
        return GFX_FAILURE;
    }

    return GFX_SUCCESS;
}

uint8_t assets_get_layout_tile(uint8_t x, uint8_t y)
{
    const uint8_t* map = (const uint8_t*)&_ztm_checkers_start;

    if (x >= SCREEN_TILE_W || y >= SCREEN_TILE_H) {
        return EMPTY_TILE;
    }

    return map[((uint16_t)y * SCREEN_TILE_W) + x];
}

void __assets__(void) __naked
{
    __asm__(
        "__ztm_checkers_start:\n"
        "    .incbin \"assets/checkers.ztm\"\n"
        "__ztm_checkers_end:\n"
    );
}
