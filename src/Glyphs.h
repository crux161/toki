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

// Indices into g_glyph_bits: 0-9 are the digits, then the colon.
#define GLYPH_SEPARATOR 10

extern const uint8_t g_glyph_bits[GLYPH_COUNT][GLYPH_HEIGHT][GLYPH_ROW_BYTES];
