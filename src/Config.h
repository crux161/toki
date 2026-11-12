#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * Theme and effect settings, loaded from a config file at start-up.
 *
 * The colours are stored already converted to minifb's pixel format, so they
 * can be written straight into a FrameBuffer without further translation.
 */

#define CONFIG_THEME_NAME_MAX 32
#define CONFIG_MAX_THEMES     8

// A named foreground/background pair.
typedef struct {
	char name[CONFIG_THEME_NAME_MAX];
	uint32_t foreground;
	uint32_t background;
} Theme;

typedef struct {
	Theme themes[CONFIG_MAX_THEMES];
	int theme_count;
	char selected[CONFIG_THEME_NAME_MAX]; // value of the `theme` key
	uint32_t foreground;                  // resolved from `selected`
	uint32_t background;
	bool scanlines;
	bool vignette;
} Config;

// Fills cfg with the built-in defaults: green digits on dark grey, no effects.
void config_default(Config *cfg);

// Parses the config file at path into cfg, on top of whatever it already holds.
// Returns false if the file cannot be opened. Unrecognised keys and malformed
// values are reported on stderr and skipped, so a partially valid file still
// takes effect.
bool config_parse_file(Config *cfg, const char *path);

// Parses ./toki.conf, falling back to ~/.config/toki/config. Returns false if
// neither exists, leaving cfg exactly as it was.
bool config_load(Config *cfg);
