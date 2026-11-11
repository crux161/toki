/*
 * Simple clock, inspired from "progress bar" by Dave Eddy
 * Windowed port: renders a CPU framebuffer through MiniFB.
 * The terminal original is kept in toki_term.c.
 *
 * # Created
 * Author: Crux161 <egraham@koken.dev>
 * Date: November 11, 2026
 * License: MIT
 */

#define _POSIX_C_SOURCE 200809L

#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "FrameBuffer.h"
#include "Glyphs.h"

#define VERSION "1.3.0"

// SEED.md calls for a 0x00101010 background and green glyphs. MFB_RGB builds
// both without relying on the raw byte order of a hex literal.
#define BACKGROUND_COLOR MFB_RGB(0x10, 0x10, 0x10)
#define FOREGROUND_COLOR MFB_RGB(0x00, 0xff, 0x00)

static void print_help(const char *prog_name) {
	fprintf(stderr, "Usage: %s [OPTION]...\n", prog_name);
	fprintf(stderr, "Display a live clock in a window.\n\n");
	fprintf(stderr, "  -s, --scale=N    Shrink the clock face by N: 1, 2 or 4 (default 1)\n");
	fprintf(stderr, "  -h, --help       Display this help and exit\n");
	fprintf(stderr, "  -v, --version    Output version information and exit\n");
}

static void print_version(const char *prog_name) {
	fprintf(stderr, "%s %s\n", prog_name, VERSION);
	fprintf(stderr, "Copyright (C) 2026 Crux161.\n");
}

// Returns the scale factor, or 0 if text is not one of the supported factors.
static int parse_scale(const char *text) {
	char *end = NULL;
	long value = strtol(text, &end, 10);

	if (end == text || *end != '\0') {
		return 0; // not a number at all, or trailing junk like "2x"
	}
	if (value != 1 && value != 2 && value != 4) {
		return 0;
	}
	return (int) value;
}

// Draws HH:MM in 24-hour time as five bundled glyphs, each centred in its cell.
static void render_clock(FrameBuffer *fb, int scale) {
	time_t now = time(NULL);
	struct tm tm_info;
	localtime_r(&now, &tm_info);

	char face[CLOCK_CELLS + 1]; // "HH:MM"
	snprintf(face, sizeof(face), "%02d:%02d", tm_info.tm_hour, tm_info.tm_min);

	const int cell = BASE_CELL_WIDTH / scale;
	const int glyph_width = GLYPH_WIDTH_AT(scale);
	const int glyph_height = GLYPH_HEIGHT_AT(scale);
	const int glyph_y = ((int) fb->height - glyph_height) / 2;

	for (int i = 0; i < CLOCK_CELLS; i++) {
		int glyph = (face[i] == ':') ? GLYPH_SEPARATOR : face[i] - '0';
		int glyph_x = i * cell + (cell - glyph_width) / 2;
		blit_glyph(fb, glyph, glyph_x, glyph_y, FOREGROUND_COLOR, scale);
	}
}

int main(int argc, char **argv) {
	static struct option const long_options[] = {
		{"scale", required_argument, NULL, 's'},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'v'},
		{NULL, 0, NULL, 0}
	};

	int scale = 1;
	int opt;
	while ((opt = getopt_long(argc, argv, "s:hv", long_options, NULL)) != -1) {
		switch (opt) {
		case 's':
			scale = parse_scale(optarg);
			if (scale == 0) {
				fprintf(stderr, "Invalid scale '%s': use 1, 2 or 4.\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		case 'h':
			print_help(argv[0]);
			return EXIT_SUCCESS;
		case 'v':
			print_version(argv[0]);
			return EXIT_SUCCESS;
		default:
			fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
			return EXIT_FAILURE;
		}
	}

	const unsigned width = BASE_WINDOW_WIDTH / scale;
	const unsigned height = BASE_WINDOW_HEIGHT / scale;

	FrameBuffer fb;
	if (!create_framebuffer(&fb, width, height)) {
		fprintf(stderr, "Error: could not allocate %ux%u framebuffer.\n", width, height);
		return EXIT_FAILURE;
	}

	struct mfb_window *window = mfb_open_ex("Toki Clock", width, height, MFB_WF_RESIZABLE);
	if (!window) {
		fprintf(stderr, "Error: could not open window.\n");
		free_framebuffer(&fb);
		return EXIT_FAILURE;
	}

	mfb_set_target_fps(60);

	while (true) {
		clear_framebuffer(&fb, BACKGROUND_COLOR);
		render_clock(&fb, scale);

		mfb_update_state state = mfb_update_ex(window, fb.pixels, fb.width, fb.height);
		if (state != MFB_STATE_OK) {
			break; // Window closed or error encountered
		}

		if (!mfb_wait_sync(window)) {
			break;
		}
	}

	mfb_close(window);
	free_framebuffer(&fb);
	return EXIT_SUCCESS;
}
