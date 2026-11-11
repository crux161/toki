/*
 * Simple terminal clock, inspired from "progress bar" by Dave Eddy
 *
 * # Created
 * Author: Crux161 <egraham@koken.dev>
 * Date: November 10, 2026
 * License: MIT
 *
 * # Contributors
 * - Crux161 <egraham@koken.dev>
 * (Progress Bar)
 * - Dave Eddy <ysap@daveeddy.com>
 */

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define VERSION "1.1.0"

volatile sig_atomic_t g_needs_resize = 0;
static int g_columns = 80;
static bool g_use_regular_font = false;

static const char *GREEN = "\e[32m";
static const char *RST = "\e[0m";

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

enum Number {
	NUM_0 = 0,
	NUM_1,
	NUM_2,
	NUM_3,
	NUM_4,
	NUM_5,
	NUM_6,
	NUM_7,
	NUM_8,
	NUM_9,
	NUM_COLON,
	NUM_NONE,
};

/* Improved 3x2 Block Font Array for High Legibility */
void print_big_number(int i) {
	static const char *font[][2] = {
		{"█▀█", "█▄█"}, // 0
		{" █ ", " █ "}, // 1
		{"▀▀█", "█▀▀"}, // 2
		{"▀▀█", "▀▄█"}, // 3
		{"█ █", "▀▀█"}, // 4
		{"█▀▀", "▀▄█"}, // 5
		{"█▀▀", "█▄█"}, // 6
		{"▀▀█", "  █"}, // 7
		{"█▀█", "█▄█"}, // 8
		{"█▀█", "▀▀█"}, // 9
		{" ▄ ", " ▄ "}, // Colon
		{"   ", "   "}, // None
	};
	fprintf(stderr, "%s", font[i][0]);
	fprintf(stderr, "\e[1B\e[3D");
	fprintf(stderr, "%s", font[i][1]);
	fprintf(stderr, "\e[1A\e[1C");
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
	fprintf(stderr, "  -t, --text     Display time using regular font instead of block glyphs\n");
	fprintf(stderr, "  -h, --help     Display this help and exit\n");
	fprintf(stderr, "  -v, --version  Output version information and exit\n");
}

static void print_version(const char *prog_name) {
	fprintf(stderr, "%s %s\n", prog_name, VERSION);
	fprintf(stderr, "Copyright (C) 2026 Crux161.\n");
	fprintf(stderr, "License MIT: <https://opensource.org/licenses/MIT>.\n");
}

static void draw_static_ui() {
	const char *WE = "─";
	const char *NS = "│";
	const char *SE = "┌";
	const char *NE = "└";
	const char *SW = "┐";
	const char *NW = "┘";

	restore_cursor();

	fprintf(stderr, "%s", GREEN);

	// Top border
	fprintf(stderr, "%s", SE);
	repeat_text(WE, g_columns - 2);
	fprintf(stderr, "%s", SW);
	fprintf(stderr, "\n");

	// Empty interior rows
	for (int i = 0; i < 2; i++) {
		fprintf(stderr, "%s", NS);
		repeat_text(" ", g_columns - 2);
		fprintf(stderr, "%s", NS);
		fprintf(stderr, "\n");
	}

	// Bottom border
	fprintf(stderr, "%s", NE);
	repeat_text(WE, g_columns - 2);
	fprintf(stderr, "%s", NW);
	fprintf(stderr, "\n");

	fprintf(stderr, "%s", RST);
}

void clear_old_screen() {
	restore_cursor();

	for (int i = 0; i < 4; i++) {
		fprintf(stderr, "\e[2K\e[1B");
	}
	fflush(stderr);
}

static void render_clock() {
	time_t t = time(NULL);
	struct tm *tm_info = localtime(&t);

	int hour = tm_info->tm_hour;
	int min = tm_info->tm_min;
	int sec = tm_info->tm_sec;

	restore_cursor();
	fprintf(stderr, "%s", GREEN);

	if (g_use_regular_font) {
		char time_str[32];
		snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", hour, min, sec);
		int len = 8; // length of HH:MM:SS
		int inner_width = g_columns - 2;
		int offset = (inner_width - len) / 2;
		if (offset < 0) offset = 0;

		fprintf(stderr, "\e[1B\e[%dC%s", 1 + offset, time_str);
	} else {
		int h1 = hour / 10;
		int h2 = hour % 10;
		int m1 = min / 10;
		int m2 = min % 10;
		int s1 = sec / 10;
		int s2 = sec % 10;

		// 8 glyphs total (6 digits + 2 colons) * 3 columns = 24 width
		int inner_width = g_columns - 2;
		int offset = (inner_width - 24) / 2;
		if (offset < 0) offset = 0;

		fprintf(stderr, "\e[1B\e[%dC", 1 + offset);

		print_big_number(h1);
		print_big_number(h2);
		print_big_number(NUM_COLON);
		print_big_number(m1);
		print_big_number(m2);
		print_big_number(NUM_COLON);
		print_big_number(s1);
		print_big_number(s2);
	}

	fprintf(stderr, "%s", RST);

	restore_cursor();
	fprintf(stderr, "\e[4B");
	fflush(stderr);
}

int main(int argc, char **argv) {
	static struct option const long_options[] = {
		{"text", no_argument, NULL, 't'},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'v'},
		{NULL, 0, NULL, 0}
	};

	int opt;
	while ((opt = getopt_long(argc, argv, "thv", long_options, NULL)) != -1) {
		switch (opt) {
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

	signal(SIGWINCH, handle_sigwinch);
	get_window_size();

	fprintf(stderr, "\n\n\n\n");
	fflush(stderr);
	fprintf(stderr, "\e[4A");
	save_cursor();
	draw_static_ui();

	while (true) {
		if (g_needs_resize) {
			get_window_size();
			g_needs_resize = 0;
			clear_old_screen();
			draw_static_ui();
		}

		render_clock();
		sleep(1);
	}

	return EXIT_SUCCESS;
}