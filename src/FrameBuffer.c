/*
 * CPU-side pixel buffer for the windowed clock, handed to MiniFB each frame.
 *
 * # Created
 * Author: Crux161 <egraham@koken.dev>
 * Date: November 11, 2026
 * License: MIT
 */

#include "FrameBuffer.h"
#include "Glyphs.h"

bool create_framebuffer(FrameBuffer *fb, uint32_t width, uint32_t height) {
    size_t count = (size_t) width * height;

    fb->width = 0;
    fb->height = 0;
    fb->pixels = NULL;

    // calloc rather than malloc so a buffer that is displayed before the first
    // clear_framebuffer holds defined pixels instead of indeterminate ones
    fb->pixels = calloc(count, sizeof(uint32_t));
    if (!fb->pixels) {
        return false;
    }

    fb->width = width;
    fb->height = height;
    return true;
}

void free_framebuffer(FrameBuffer *fb) {
    free(fb->pixels);
    fb->width = 0;
    fb->height = 0;
    fb->pixels = NULL;
}

void clear_framebuffer(FrameBuffer *fb, uint32_t color) {
    size_t count = (size_t) fb->width * fb->height;

    for (size_t i = 0; i < count; i++) {
        fb->pixels[i] = color;
    }
}

void draw_pixel(FrameBuffer *fb, int x, int y, uint32_t color) {
    if (x < 0 || x >= (int) fb->width || y < 0 || y >= (int) fb->height) {
        return;
    }

    size_t idx = (size_t) y * fb->width + (size_t) x;
    fb->pixels[idx] = color;
}

void blit_glyph(FrameBuffer *fb, int glyph, int x, int y, uint32_t fg_color, int scale) {
    if (glyph < 0 || glyph >= GLYPH_COUNT) {
        return;
    }
    if (scale < 1) {
        scale = 1;
    }

    const int width = GLYPH_WIDTH_AT(scale);
    const int height = GLYPH_HEIGHT_AT(scale);

    for (int row = 0; row < height; row++) {
        // Point sampling: each destination pixel takes the source pixel `scale`
        // steps further in. The thinnest stroke in the art is 6px wide, so even
        // a step of 4 always lands inside a stroke and no feature disappears.
        const uint8_t *bits = g_glyph_bits[glyph][row * scale];

        for (int col = 0; col < width; col++) {
            int src_x = col * scale;
            // Rows are packed MSB first, so bit 7 is the leftmost pixel.
            if ((bits[src_x >> 3] >> (7 - (src_x & 7))) & 1) {
                draw_pixel(fb, x + col, y + row, fg_color);
            }
        }
    }
}

#define SCANLINE_DIM      0.85f // odd rows lose 15% of their luminance
#define VIGNETTE_STRENGTH 0.25f // corners fall to 50% brightness

void apply_effects(FrameBuffer *fb, bool scanlines, bool vignette) {
    if (!scanlines && !vignette) {
        return;
    }

    const float centre_x = fb->width * 0.5f;
    const float centre_y = fb->height * 0.5f;

    for (uint32_t y = 0; y < fb->height; y++) {
        float row_factor = (scanlines && (y % 2) != 0) ? SCANLINE_DIM : 1.0f;
        // The vignette's vertical term depends only on the row, so hoist it.
        float dy = vignette ? ((float) y - centre_y) / centre_y : 0.0f;

        for (uint32_t x = 0; x < fb->width; x++) {
            float factor = row_factor;

            if (vignette) {
                float dx = ((float) x - centre_x) / centre_x;
                float weight = 1.0f - (dx * dx + dy * dy) * VIGNETTE_STRENGTH;
                factor *= (weight > 0.0f) ? weight : 0.0f;
            }

            if (factor >= 1.0f) {
                continue; // nothing to dim, which skips every even row wholesale
            }

            // MFB_GET_* and MFB_RGB rather than the fixed 16/8/0 shifts in
            // theme_proposal.md: those shifts are right on desktop, where pixels
            // are 0xAARRGGBB, but wrong on Android, and they also drop alpha.
            size_t idx = (size_t) y * fb->width + x;
            uint32_t color = fb->pixels[idx];
            fb->pixels[idx] = MFB_RGB((uint32_t) (MFB_GET_R(color) * factor),
                                      (uint32_t) (MFB_GET_G(color) * factor),
                                      (uint32_t) (MFB_GET_B(color) * factor));
        }
    }
}
