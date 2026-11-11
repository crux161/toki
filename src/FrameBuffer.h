#pragma once

#include <MiniFB.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// clock face is NN:NN or 72 pixels * 5 spaces or 360 pixels wide, by 72 pixels high

#define WINDOW_WIDTH  360
#define WINDOW_HEIGHT 72

// 32-bit pixel buffer. Build colours with MFB_RGB() / MFB_ARGB() rather than
// writing hex literals: on desktop minifb packs them as 0xAABBGGRR, so a
// conventional 0x00RRGGBB literal has red and blue swapped.
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
