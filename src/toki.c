/*
 * Simple terminal clock, inspired from "progress bar" by Dave Eddy
 * Refactored for Chaitea terminal primitives.
 *
 * # Created
 * Author: Crux161 <egraham@koken.dev>
 * Date: November 11, 2026
 * License: MIT
 */

#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <chaitea.h>

#define VERSION "1.3.0"
#define DEFAULT_FONT_PATH "resources/fonts/Tmplr.flf"

volatile sig_atomic_t g_needs_resize = 0;
static int g_columns = 80;
static bool g_use_regular_font = false;
static char *g_font_path = NULL;

typedef struct {
	int height;
	int char_height;
	char *glyph[11]; // 0-9 indices, 10 for colon
} FigletFont;

static FigletFont g_figfont;
static bool g_font_loaded = false;

void handle_sigwinch(int sig) {
	(void)sig;
	g_needs_resize = 1;
}

void get_window_size() {
	struct winsize ws;
	if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == -1) {
		return;
	}
	g_columns = ws.ws_col;
	if (g_columns < 30) {
		g_columns = 30;
	}
}

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

static void save_cursor() {
	fprintf(stderr, "\e[s");
}

static void restore_cursor() {
	fprintf(stderr, "\e[u");
}

static void repeat_text(const char *s, int times) {
	for (int i = 0; i < times; i++) {
		fprintf(stderr, "%s", s);
	}
}

static void print_help(const char *prog_name) {
	fprintf(stderr, "Usage: %s [OPTION]...\n", prog_name);
	fprintf(stderr, "Display a live terminal clock inside a framed box.\n\n");
	fprintf(stderr, "  -f, --font=PATH  Path to custom FIGlet .flf font file\n");
	fprintf(stderr, "  -t, --text       Display time using regular font instead of glyphs\n");
	fprintf(stderr, "  -h, --help       Display this help and exit\n");
	fprintf(stderr, "  -v, --version    Output version information and exit\n");
}

static void print_version(const char *prog_name) {
	fprintf(stderr, "%s %s\n", prog_name, VERSION);
	fprintf(stderr, "Copyright (C) 2026 Crux161.\n");
}

static void draw_static_ui(int box_height, Terminal term) {
	const char *WE = "─";
	const char *NS = "│";
	const char *SE = "┌";
	const char *NE = "└";
	const char *SW = "┐";
	const char *NW = "┘";

	restore_cursor();
	setFG(term, 2); // Green color via chaitea

	fprintf(stderr, "%s", SE);
	repeat_text(WE, g_columns - 2);
	fprintf(stderr, "%s", SW);
	fprintf(stderr, "\n");

	for (int i = 0; i < box_height; i++) {
		fprintf(stderr, "%s", NS);
		repeat_text(" ", g_columns - 2);
		fprintf(stderr, "%s", NS);
		fprintf(stderr, "\n");
	}

	fprintf(stderr, "%s", NE);
	repeat_text(WE, g_columns - 2);
	fprintf(stderr, "%s", NW);
	fprintf(stderr, "\n");

	resetStyle(term);
}

void clear_old_screen(int box_height) {
	restore_cursor();
	for (int i = 0; i < box_height + 2; i++) {
		fprintf(stderr, "\e[2K\e[1B");
	}
	fflush(stderr);
}

static void render_clock(int box_height, Terminal term) {
	time_t t = time(NULL);
	struct tm *tm_info = localtime(&t);

	int hour = tm_info->tm_hour;
	int min = tm_info->tm_min;
	int sec = tm_info->tm_sec;

	restore_cursor();
	setFG(term, 2);

	if (g_use_regular_font || !g_font_loaded) {
		char time_str[32];
		snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", hour, min, sec);
		int len = 8;
		int inner_width = g_columns - 2;
		int offset = (inner_width - len) / 2;
		if (offset < 0) offset = 0;

		fprintf(stderr, "\e[1B\e[2K\e[%dC%s", 1 + offset, time_str);
	} else {
		char time_chars[9];
		snprintf(time_chars, sizeof(time_chars), "%02d:%02d:%02d", hour, min, sec);

		fprintf(stderr, "\e[1B");

		for (int r = 0; r < g_figfont.height; r++) {
			char row_buf[1024] = {0};

			for (int c = 0; c < 8; c++) {
				int idx = (time_chars[c] == ':') ? 10 : (time_chars[c] - '0');
				if (idx >= 0 && idx <= 10 && g_figfont.glyph[idx]) {
					char *glyph = g_figfont.glyph[idx];
					int current_row = 0;
					char *line_start = glyph;
					char line_str[256] = {0};

					for (char *p = glyph; ; p++) {
						if (*p == '\n' || *p == '\0') {
							if (current_row == r) {
								int len = p - line_start;
								if (len >= (int)sizeof(line_str)) len = sizeof(line_str) - 1;
								memcpy(line_str, line_start, len);
								line_str[len] = '\0';
								break;
							}
							current_row++;
							line_start = p + 1;
							if (*p == '\0') break;
						}
					}
					strcat(row_buf, line_str);
				}
			}

			fprintf(stderr, "\e[2K");
			fprintf(stderr, " \e[2C%s\n", row_buf);
		}
	}

	resetStyle(term);
	restore_cursor();
	fprintf(stderr, "\e[%dB", box_height + 2);
	fflush(stderr);
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

	if (!g_use_regular_font) {
		const char *path_to_try = g_font_path ? g_font_path : DEFAULT_FONT_PATH;
		g_font_loaded = load_figlet_font(path_to_try);
		if (!g_font_loaded) {
			fprintf(stderr, "Warning: Falling back to regular text font.\n");
		}
	}

	int box_height = g_font_loaded ? g_figfont.height : 2;

	Terminal term = getHandle(stdin, stderr);

	signal(SIGWINCH, handle_sigwinch);
	get_window_size();

	fprintf(stderr, "\n");
	for (int i = 0; i < box_height + 1; i++) fprintf(stderr, "\n");
	fflush(stderr);
	fprintf(stderr, "\e[%dA", box_height + 2);
	save_cursor();
	draw_static_ui(box_height, term);

	while (true) {
		if (g_needs_resize) {
			get_window_size();
			g_needs_resize = 0;
			clear_old_screen(box_height);
			draw_static_ui(box_height, term);
		}

		render_clock(box_height, term);
		sleep(1);
	}

	releaseTerm(term);
	return EXIT_SUCCESS;
}
