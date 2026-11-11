#include <MiniFB.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// clock face is NN:NN or 72 pixels * 5 spaces or 360 pixels wide, by 72 pixels high

#define WINDOW_WIDTH  360
#define WINDOW_HEIGHT 72

// ARGB/RGB32 pixel buffer representation (0x00RRGGBB or 0xFFRRGGBB)
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t *pixels; // Allocated array of size (width * height)
} FrameBuffer;
