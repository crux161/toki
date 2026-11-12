#pragma once

#include <MiniFB.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Clock face geometry at scale 1: NN:NN, five 72-pixel cells, 360x72 overall.
// Every length below is a 1x size — divide by the scale factor for the real one.
// They all divide evenly by the supported factors of 1, 2 and 4.

#define BASE_WINDOW_WIDTH  360
#define BASE_WINDOW_HEIGHT 72

// The five NN:MM character cells; each glyph is centred in its own cell.
#define CLOCK_CELLS        5
#define BASE_CELL_WIDTH    (BASE_WINDOW_WIDTH / CLOCK_CELLS)

// 32-bit pixel buffer. Build colours with MFB_RGB() / MFB_ARGB() and read them
// back with MFB_GET_* rather than writing hex literals: the channel layout is
// 0xAARRGGBB on desktop but 0xAABBGGRR on Android, and the macros also fill in
// the alpha that a 0x00RRGGBB literal leaves at zero.
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t *pixels; // Allocated array of size (width * height)
} FrameBuffer;

// Allocates fb->pixels for width * height pixels and records the dimensions.
// Returns false and leaves fb zeroed if the allocation fails.
bool create_framebuffer(FrameBuffer *fb, uint32_t width, uint32_t height);

// Releases fb->pixels and zeroes fb, so a second call is harmless.
void free_framebuffer(FrameBuffer *fb);

// Fills every pixel with color.
void clear_framebuffer(FrameBuffer *fb, uint32_t color);

// Plots one pixel, ignoring coordinates that fall outside the buffer.
void draw_pixel(FrameBuffer *fb, int x, int y, uint32_t color);

// Blits one glyph, by index into g_glyph_bits, with its top-left at (x, y) and
// downscaled by an integer factor: 1 draws the native 69x71, while 2 and 4
// point-sample the source, giving 35x36 and 18x18. Set bits are painted
// fg_color; clear bits leave the existing pixel alone, so the background shows
// through. Off-buffer pixels are clipped, and a factor below 1 is treated as 1.
void blit_glyph(FrameBuffer *fb, int glyph, int x, int y, uint32_t fg_color, int scale);

// Applies the CRT effects from theme_proposal.md to the whole composited frame:
// scanlines dim odd rows by 15%, and the vignette applies a radial falloff from
// the centre that reaches 50% at the corners. Doing this after the glyphs are
// drawn, rather than during the blit, is what lets both effects reach the
// background as well. Either flag may be false, and both off is a no-op.
void apply_effects(FrameBuffer *fb, bool scanlines, bool vignette);
