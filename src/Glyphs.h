#pragma once

#include <stdint.h>

/*
 * Glyph bitmaps, bundled into the binary by scripts/gen_glyphs.py so the clock
 * renders without reading resources/glyphs/ at run time.
 *
 * One bit per pixel, MSB first, GLYPH_ROW_BYTES bytes per row; a set bit is a
 * foreground pixel. Regenerate with: python3 scripts/gen_glyphs.py
 */

#define GLYPH_WIDTH     69
#define GLYPH_HEIGHT    71
#define GLYPH_ROW_BYTES ((GLYPH_WIDTH + 7) / 8)
#define GLYPH_COUNT     11

// Destination size of a glyph drawn at an integer downscale factor, rounded up
// so the whole source glyph is covered. blit_glyph point-samples the source at
// (col * scale, row * scale), and rounding up keeps the last sample inside the
// bitmap: at scale 4 that is (18 - 1) * 4 = 68, within 69x71.
#define GLYPH_WIDTH_AT(scale)  ((GLYPH_WIDTH  + (scale) - 1) / (scale))
#define GLYPH_HEIGHT_AT(scale) ((GLYPH_HEIGHT + (scale) - 1) / (scale))

// Indices into g_glyph_bits: 0-9 are the digits, then the colon.
#define GLYPH_SEPARATOR 10

extern const uint8_t g_glyph_bits[GLYPH_COUNT][GLYPH_HEIGHT][GLYPH_ROW_BYTES];
