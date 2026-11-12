/*
 * Kitty graphics protocol backend: FrameBuffer -> APC escape sequences.
 * See kitty-graphics.mdx and https://sw.kovidgoyal.net/kitty/graphics-protocol/
 * for the protocol this speaks.
 *
 * # Created
 * Author: Crux161 <egraham@koken.dev>
 * Date: November 11, 2026
 * License: MIT
 */

#define _POSIX_C_SOURCE 200809L

#include "KittyRender.h"

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

// The identity of the clock's single image and its single placement. Both
// staying fixed is what makes an update replace rather than accumulate: the
// protocol deletes an image's placements when its data is re-transmitted, and
// a display with the same (image, placement) ids replaces the old placement.
#define IMAGE_ID     1
#define PLACEMENT_ID 1

// Base64 characters per chunk. The protocol caps a chunk at 4096 bytes and
// requires every non-final chunk to be a multiple of 4 long; 4096 is both.
#define CHUNK_LEN 4096

static const char BASE64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Scratch and diff state, all empty until kitty_render_start succeeds.
static uint8_t *g_rgb;       // the current frame as f=24 pixel bytes
static uint8_t *g_prev_rgb;  // the last frame successfully transmitted
static char *g_base64;       // g_rgb, base64-encoded
static size_t g_rgb_size;
static size_t g_base64_size;
static bool g_started;

static size_t base64_size(size_t bytes) {
    return (bytes + 2) / 3 * 4;
}

static void base64_encode(const uint8_t *src, size_t len, char *dst) {
    size_t out = 0;

    for (size_t i = 0; i + 3 <= len; i += 3) {
        uint32_t triple = (uint32_t) src[i] << 16 | (uint32_t) src[i + 1] << 8 | src[i + 2];
        dst[out++] = BASE64[(triple >> 18) & 0x3F];
        dst[out++] = BASE64[(triple >> 12) & 0x3F];
        dst[out++] = BASE64[(triple >> 6) & 0x3F];
        dst[out++] = BASE64[triple & 0x3F];
    }

    // The tail is one or two bytes; '=' pads the quantum out to four chars.
    size_t left = len % 3;
    if (left == 1) {
        uint32_t triple = (uint32_t) src[len - 1] << 16;
        dst[out++] = BASE64[(triple >> 18) & 0x3F];
        dst[out++] = BASE64[(triple >> 12) & 0x3F];
        dst[out++] = '=';
        dst[out++] = '=';
    } else if (left == 2) {
        uint32_t triple = ((uint32_t) src[len - 2] << 16) | ((uint32_t) src[len - 1] << 8);
        dst[out++] = BASE64[(triple >> 18) & 0x3F];
        dst[out++] = BASE64[(triple >> 12) & 0x3F];
        dst[out++] = BASE64[(triple >> 6) & 0x3F];
        dst[out++] = '=';
    }
}

// The f=24 payload: rows top to bottom, each pixel three bytes R, G, B.
static void framebuffer_to_rgb(const FrameBuffer *fb) {
    size_t pixels = (size_t) fb->width * fb->height;

    for (size_t i = 0; i < pixels; i++) {
        uint32_t color = fb->pixels[i];
        g_rgb[i * 3 + 0] = MFB_GET_R(color);
        g_rgb[i * 3 + 1] = MFB_GET_G(color);
        g_rgb[i * 3 + 2] = MFB_GET_B(color);
    }
}

static bool write_all(const void *data, size_t len) {
    const char *p = data;

    while (len > 0) {
        size_t written = fwrite(p, 1, len, stdout);
        if (written == 0) {
            return false;
        }
        p += written;
        len -= written;
    }
    return true;
}

// Sends g_rgb as one a=t transmission, split into protocol-sized chunks. Only
// the first chunk carries the full control data; later ones carry just m —
// and q, which suppresses the OK replies the terminal would otherwise queue
// on stdin, where nothing would ever read them. The final chunk drops m (or
// sets it to 0), which completes the image.
static bool emit_transmit(uint32_t width, uint32_t height) {
    for (size_t off = 0; off < g_base64_size; off += CHUNK_LEN) {
        size_t len = g_base64_size - off;
        if (len > CHUNK_LEN) {
            len = CHUNK_LEN;
        }
        bool final = off + len == g_base64_size;

        char control[64];
        int control_len;
        if (off == 0) {
            control_len = snprintf(control, sizeof(control),
                                   "\x1b_Ga=t,i=%d,f=24,s=%u,v=%u,q=1%s;",
                                   IMAGE_ID, width, height, final ? "" : ",m=1");
        } else {
            control_len = snprintf(control, sizeof(control),
                                   "\x1b_Gq=1,m=%d;", final ? 0 : 1);
        }
        if (control_len < 0 || (size_t) control_len >= sizeof(control)) {
            return false;
        }

        if (!write_all(control, (size_t) control_len) ||
            !write_all(g_base64 + off, len) ||
            !write_all("\x1b\\", 2)) {
            return false;
        }
    }
    return true;
}

// Displays IMAGE_ID at the cursor without moving the cursor, so the clock
// stays anchored where it started and the shell is not pushed sideways.
static bool emit_placement(void) {
    char control[48];
    int len = snprintf(control, sizeof(control),
                       "\x1b_Ga=p,i=%d,p=%d,C=1,q=1\x1b\\",
                       IMAGE_ID, PLACEMENT_ID);
    if (len < 0 || (size_t) len >= sizeof(control)) {
        return false;
    }
    return write_all(control, (size_t) len);
}

// Encodes and sends the frame in g_rgb, then records it in g_prev_rgb. The
// caller must have filled g_rgb with framebuffer_to_rgb first.
static bool send_current(uint32_t width, uint32_t height) {
    base64_encode(g_rgb, g_rgb_size, g_base64);

    if (!emit_transmit(width, height) || !emit_placement() || fflush(stdout) == EOF) {
        fprintf(stderr, "Error: writing to the terminal failed.\n");
        return false;
    }

    memcpy(g_prev_rgb, g_rgb, g_rgb_size);
    return true;
}

// The support handshake the spec prescribes: a graphics query followed by a
// primary device attributes request. A terminal implementing the graphics
// protocol answers the query; any other terminal answers only the device
// attributes, so which reply arrives is the answer. Reading until both have
// arrived also drains them, leaving nothing behind in the line buffer for the
// shell to trip over once toki exits.
static bool query_support(void) {
    struct termios original;
    if (tcgetattr(STDIN_FILENO, &original) != 0) {
        return false;
    }

    // No ECHO, so the replies do not land on screen; no ICANON, so they are
    // readable the moment they arrive rather than after a newline.
    struct termios probe = original;
    probe.c_lflag &= ~(ECHO | ICANON);
    probe.c_cc[VMIN] = 0;
    probe.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &probe) != 0) {
        return false;
    }

    fputs("\x1b_Gi=31,s=1,v=1,a=q,t=d,f=24;AAAA\x1b\\\x1b[c", stdout);
    fflush(stdout);

    char replies[256];
    size_t have = 0;
    bool graphics = false;
    bool attributes = false;

    // Replies land inside the first 100ms slice; the remaining slices are
    // tolerance for slower terminals, and one silent slice ends the wait.
    for (int slice = 0; slice < 20 && !(graphics && attributes); slice++) {
        struct pollfd fd = { .fd = STDIN_FILENO, .events = POLLIN };
        if (poll(&fd, 1, 100) == 0) {
            break;
        }

        ssize_t got = read(STDIN_FILENO, replies + have, sizeof(replies) - 1 - have);
        if (got <= 0) {
            break;
        }
        have += (size_t) got;
        replies[have] = '\0'; // replies are ESC plus printable ASCII

        graphics = strstr(replies, "\x1b_G") != NULL;
        const char *csi = strstr(replies, "\x1b[?");
        attributes = csi != NULL && strchr(csi, 'c') != NULL;
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &original);
    return graphics;
}

bool kitty_render_start(const FrameBuffer *fb) {
    if (!isatty(STDOUT_FILENO) || !isatty(STDIN_FILENO)) {
        fprintf(stderr, "Error: kitty output needs toki running on a terminal, not a pipe.\n");
        return false;
    }

    if (!query_support()) {
        fprintf(stderr, "Error: this terminal does not support the kitty graphics protocol.\n");
        return false;
    }

    // From here on the terminal is being mutated, so kitty_render_end has
    // real work to do on every failure path below.
    g_started = true;

    g_rgb_size = (size_t) fb->width * fb->height * 3;
    g_base64_size = base64_size(g_rgb_size);
    g_rgb = malloc(g_rgb_size);
    g_prev_rgb = malloc(g_rgb_size);
    g_base64 = malloc(g_base64_size);
    if (!g_rgb || !g_prev_rgb || !g_base64) {
        fprintf(stderr, "Error: could not allocate a %ux%u kitty frame.\n",
                fb->width, fb->height);
        kitty_render_end();
        return false;
    }

    fputs("\x1b[?25l", stdout); // keep the cursor from blinking over the clock

    framebuffer_to_rgb(fb);
    if (!send_current(fb->width, fb->height)) {
        kitty_render_end();
        return false;
    }
    return true;
}

bool kitty_render_frame(const FrameBuffer *fb) {
    if (!g_started) {
        return false;
    }

    framebuffer_to_rgb(fb);
    if (memcmp(g_rgb, g_prev_rgb, g_rgb_size) == 0) {
        return true; // same minute, same pixels: nothing to send
    }
    return send_current(fb->width, fb->height);
}

void kitty_render_end(void) {
    if (g_started) {
        // d=I drops every placement of the image and frees its data. It also
        // aborts a chunked upload that a failed write left half-finished.
        char control[32];
        int len = snprintf(control, sizeof(control), "\x1b_Ga=d,d=I,i=%d\x1b\\", IMAGE_ID);
        if (len > 0 && (size_t) len < sizeof(control)) {
            fputs(control, stdout);
        }
        fputs("\x1b[?25h", stdout); // give the cursor back
        fputs("\r\n", stdout);      // step past the cleared image area
        fflush(stdout);
    }

    free(g_rgb);
    free(g_prev_rgb);
    free(g_base64);
    g_rgb = NULL;
    g_prev_rgb = NULL;
    g_base64 = NULL;
    g_rgb_size = 0;
    g_base64_size = 0;
    g_started = false;
}
