#pragma once

#include "FrameBuffer.h"
#include <stdbool.h>

/*
 * Output backend that renders a FrameBuffer into the terminal using the kitty
 * graphics protocol: APC escape sequences carrying the frame as chunked,
 * base64-encoded 24-bit RGB pixels.
 *
 * A single image id is reused for the whole run. Re-transmitting the data for
 * that id is how a frame update works — the protocol replaces the image and
 * drops its placements, so the frame is immediately re-placed with the same
 * (image, placement) ids, which replaces rather than stacks. Transmissions are
 * limited to frames whose pixels actually changed, which for a HH:MM face is
 * one second per minute.
 *
 * kitty_render_end must be called exactly once after a successful
 * kitty_render_start — including on the way out from a signal — because it
 * deletes the image and restores the terminal. After a failed start it is a
 * harmless no-op, so it can go on every error path.
 */

// Checks that stdin and stdout are a terminal that speaks the protocol (the
// spec's query plus device-attributes handshake), then transmits and displays
// the frame currently in fb. On failure the reason is printed on stderr and
// the terminal is restored, so callers only need to give up.
bool kitty_render_start(const FrameBuffer *fb);

// Transmits and re-places the image if fb differs from the last frame sent,
// and does nothing otherwise. Returns false once writing to the terminal has
// failed, meaning the run is over; the reason is printed on stderr.
bool kitty_render_frame(const FrameBuffer *fb);

// Deletes the clock's image — data and placements — shows the cursor again,
// moves below the cleared image area and releases the internal buffers. Safe
// to call without a successful start.
void kitty_render_end(void);
