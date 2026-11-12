/*
 * Simple clock, inspired from "progress bar" by Dave Eddy
 * Windowed port: renders a CPU framebuffer through MiniFB, or straight into
 * the terminal with the kitty graphics protocol. The terminal original is
 * kept in toki_term.c.
 *
 * # Created
 * Author: Crux161 <egraham@koken.dev>
 * Date: November 11, 2026
 * License: MIT
 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "Config.h"
#include "FrameBuffer.h"
#include "Glyphs.h"
#include "KittyRender.h"

#define VERSION "1.3.0"

static void print_help(const char *prog_name) {
	fprintf(stderr, "Usage: %s [OPTION]...\n", prog_name);
	fprintf(stderr, "Display a live clock in a window or a terminal.\n\n");
	fprintf(stderr, "  -s, --scale=N    Shrink the clock face by N: 1, 2 or 4 (default 1)\n");
	fprintf(stderr, "  -k, --kitty      Draw in the terminal with the kitty graphics\n");
	fprintf(stderr, "                   protocol instead of opening a window\n");
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
static void render_clock(FrameBuffer *fb, int scale, uint32_t fg_color) {
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
		blit_glyph(fb, glyph, glyph_x, glyph_y, fg_color, scale);
	}
}

// One frame of the face, shared by both output backends.
static void render_frame(FrameBuffer *fb, const Config *config, int scale) {
	clear_framebuffer(fb, config->background);
	render_clock(fb, scale, config->foreground);
	apply_effects(fb, config->scanlines, config->vignette);
}

// The window backend: the 60fps minifb loop.
static int run_window(const Config *config, FrameBuffer *fb, int scale) {
	struct mfb_window *window = mfb_open_ex("Toki Clock", fb->width, fb->height,
	                                        MFB_WF_RESIZABLE);
	if (!window) {
		fprintf(stderr, "Error: could not open window.\n");
		return EXIT_FAILURE;
	}

	mfb_set_target_fps(60);

	while (true) {
		render_frame(fb, config, scale);

		mfb_update_state state = mfb_update_ex(window, fb->pixels, fb->width, fb->height);
		if (state != MFB_STATE_OK) {
			break; // Window closed or error encountered
		}

		if (!mfb_wait_sync(window)) {
			break;
		}
	}

	mfb_close(window);
	return EXIT_SUCCESS;
}

// Set by the stop-signal handler so the kitty loop can tear the image down on
// the way out instead of dying mid-escape-sequence.
static volatile sig_atomic_t g_running = 1;

static void handle_stop(int signo) {
	(void) signo;
	g_running = 0;
}

// No SA_RESTART on purpose: the kitty loop relies on nanosleep returning
// EINTR to notice the flag before the next second is up.
static void install_stop_handlers(void) {
	struct sigaction action = {
		.sa_handler = handle_stop,
	};
	sigemptyset(&action.sa_mask);

	if (sigaction(SIGINT, &action, NULL) != 0 || sigaction(SIGTERM, &action, NULL) != 0) {
		perror("Warning: could not install signal handlers");
	}
}

// The terminal backend: wake once a second, re-render, and let KittyRender
// transmit only when the face actually changed.
static int run_kitty(const Config *config, FrameBuffer *fb, int scale) {
	install_stop_handlers();

	render_frame(fb, config, scale);
	if (!kitty_render_start(fb)) {
		return EXIT_FAILURE;
	}

	bool ok = true;
	while (g_running) {
		struct timespec nap = { .tv_sec = 1 };
		while (nanosleep(&nap, &nap) == -1 && errno == EINTR && g_running) {
			// interrupted: sleep out the remainder, or stop on the flag
		}
		if (!g_running) {
			break;
		}

		render_frame(fb, config, scale);
		if (!kitty_render_frame(fb)) {
			ok = false;
			break;
		}
	}

	kitty_render_end();
	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int main(int argc, char **argv) {
	static struct option const long_options[] = {
		{"scale", required_argument, NULL, 's'},
		{"kitty", no_argument, NULL, 'k'},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'v'},
		{NULL, 0, NULL, 0}
	};

	int scale = 1;
	bool kitty = false;
	int opt;
	while ((opt = getopt_long(argc, argv, "s:hvk", long_options, NULL)) != -1) {
		switch (opt) {
		case 's':
			scale = parse_scale(optarg);
			if (scale == 0) {
				fprintf(stderr, "Invalid scale '%s': use 1, 2 or 4.\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		case 'k':
			kitty = true;
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

	// No config file is not an error: the defaults stay in place.
	Config config;
	config_default(&config);
	config_load(&config);

	const unsigned width = BASE_WINDOW_WIDTH / scale;
	const unsigned height = BASE_WINDOW_HEIGHT / scale;

	FrameBuffer fb;
	if (!create_framebuffer(&fb, width, height)) {
		fprintf(stderr, "Error: could not allocate %ux%u framebuffer.\n", width, height);
		return EXIT_FAILURE;
	}

	int status = kitty
		? run_kitty(&config, &fb, scale)
		: run_window(&config, &fb, scale);

	free_framebuffer(&fb);
	return status;
}
