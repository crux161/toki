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

#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FrameBuffer.h"

#define VERSION "1.3.0"
#define DEFAULT_FONT_PATH "resources/fonts/Tmplr.flf"

// SEED.md calls for a 0x00101010 background. MFB_RGB builds the same dark grey
// without relying on the raw byte order of the literal.
#define BACKGROUND_COLOR MFB_RGB(0x10, 0x10, 0x10)

static bool g_use_regular_font = false;
static char *g_font_path = NULL;

typedef struct {
	int height;
	int char_height;
	char *glyph[11]; // 0-9 indices, 10 for colon
} FigletFont;

static FigletFont g_figfont;
static bool g_font_loaded = false;

// Robust parser for standard FIGlet .flf fonts
bool load_figlet_font(const char *path) {
	FILE *f = fopen(path, "r");
	if (!f) {
		fprintf(stderr, "Error opening font file '%s': %s\n", path, strerror(errno));
		return false;
	}

	char line[2048];
	if (!fgets(line, sizeof(line), f)) {
		fclose(f);
		return false;
	}

	char magic[6] = {0};
	char hardblank = ' ';
	int height = 0, base = 0, max_len = 0, old_layout = 0, comment_lines = 0;

	if (sscanf(line, "%5s %c %d %d %d %d %d", magic, &hardblank, &height, &base, &max_len, &old_layout, &comment_lines) < 6) {
		fclose(f);
		return false;
	}

	g_figfont.height = height;
	g_figfont.char_height = height;

	for (int i = 0; i < comment_lines; i++) {
		if (!fgets(line, sizeof(line), f)) {
			fclose(f);
			return false;
		}
	}

	for (int ascii_code = 32; ascii_code < 127; ascii_code++) {
		char *glyph_buf = malloc(height * 512);
		if (!glyph_buf) {
			fclose(f);
			return false;
		}
		glyph_buf[0] = '\0';
		bool valid = true;
		char end_marker = '\0';

		for (int row = 0; row < height; row++) {
			if (!fgets(line, sizeof(line), f)) {
				valid = false;
				break;
			}

			size_t len = strlen(line);
			while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
				line[--len] = '\0';
			}

			if (row == 0 && len > 0) {
				end_marker = line[len - 1];
			}

			while (len > 0 && line[len - 1] == end_marker) {
				line[--len] = '\0';
			}

			strcat(glyph_buf, line);
			if (row < height - 1) {
				strcat(glyph_buf, "\n");
			}
		}

		if (!valid) {
			free(glyph_buf);
			break;
		}

		if (ascii_code >= '0' && ascii_code <= '9') {
			g_figfont.glyph[ascii_code - '0'] = glyph_buf;
		} else if (ascii_code == ':') {
			g_figfont.glyph[10] = glyph_buf;
		} else {
			free(glyph_buf);
		}
	}

	fclose(f);
	return true;
}

static void print_help(const char *prog_name) {
	fprintf(stderr, "Usage: %s [OPTION]...\n", prog_name);
	fprintf(stderr, "Display a live clock in a window.\n\n");
	fprintf(stderr, "  -f, --font=PATH  Path to custom FIGlet .flf font file\n");
	fprintf(stderr, "  -t, --text       Display time using regular font instead of glyphs\n");
	fprintf(stderr, "  -h, --help       Display this help and exit\n");
	fprintf(stderr, "  -v, --version    Output version information and exit\n");
}

static void print_version(const char *prog_name) {
	fprintf(stderr, "%s %s\n", prog_name, VERSION);
	fprintf(stderr, "Copyright (C) 2026 Crux161.\n");
}

int main(int argc, char **argv) {
	static struct option const long_options[] = {
		{"font", required_argument, NULL, 'f'},
		{"text", no_argument, NULL, 't'},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'v'},
		{NULL, 0, NULL, 0}
	};

	int opt;
	while ((opt = getopt_long(argc, argv, "f:thv", long_options, NULL)) != -1) {
		switch (opt) {
		case 'f':
			g_font_path = optarg;
			break;
		case 't':
			g_use_regular_font = true;
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

	// Loaded and parsed now so the glyph data is ready to blit once rendering
	// lands; nothing draws it yet, so the window is blank.
	if (!g_use_regular_font) {
		const char *path_to_try = g_font_path ? g_font_path : DEFAULT_FONT_PATH;
		g_font_loaded = load_figlet_font(path_to_try);
		if (!g_font_loaded) {
			fprintf(stderr, "Warning: Falling back to regular text font.\n");
		}
	}

	FrameBuffer fb;
	if (!create_framebuffer(&fb, WINDOW_WIDTH, WINDOW_HEIGHT)) {
		fprintf(stderr, "Error: could not allocate %ux%u framebuffer.\n", WINDOW_WIDTH, WINDOW_HEIGHT);
		return EXIT_FAILURE;
	}

	struct mfb_window *window = mfb_open_ex("Toki Clock", WINDOW_WIDTH, WINDOW_HEIGHT, MFB_WF_RESIZABLE);
	if (!window) {
		fprintf(stderr, "Error: could not open window.\n");
		free_framebuffer(&fb);
		return EXIT_FAILURE;
	}

	mfb_set_target_fps(60);

	while (true) {
		clear_framebuffer(&fb, BACKGROUND_COLOR);

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
