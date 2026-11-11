/*
 * CPU-side pixel buffer for the windowed clock, handed to MiniFB each frame.
 *
 * # Created
 * Author: Crux161 <egraham@koken.dev>
 * Date: November 11, 2026
 * License: MIT
 */

#include "FrameBuffer.h"

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
