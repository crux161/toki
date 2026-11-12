/*
 * Config file parser for theme and effect settings.
 *
 * Format is `key = value` with `#` or `;` comments, plus `[name]` sections that
 * define a theme's colours:
 *
 *     theme = cyber_phosphor
 *     scanlines = on
 *
 *     [cyber_phosphor]
 *     foreground = 00FF66
 *     background = 030A05
 *
 * # Created
 * Author: Crux161 <egraham@koken.dev>
 * Date: November 11, 2026
 * License: MIT
 */

#define _POSIX_C_SOURCE 200809L

#include "Config.h"

#include <ctype.h>
#include <MiniFB.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// The look toki had before themes existed: green digits on dark grey.
#define DEFAULT_FOREGROUND MFB_RGB(0x00, 0xff, 0x00)
#define DEFAULT_BACKGROUND MFB_RGB(0x10, 0x10, 0x10)

void config_default(Config *cfg) {
	memset(cfg, 0, sizeof(*cfg));
	cfg->foreground = DEFAULT_FOREGROUND;
	cfg->background = DEFAULT_BACKGROUND;
}

static char *trim(char *text) {
	while (*text && isspace((unsigned char) *text)) {
		text++;
	}

	char *end = text + strlen(text);
	while (end > text && isspace((unsigned char) end[-1])) {
		*--end = '\0';
	}

	return text;
}

// Accepts RRGGBB or AARRGGBB, with an optional 0x prefix. A leading '#' is not
// accepted because '#' starts a comment in this format.
//
// The components are re-encoded with MFB_RGB rather than used as a raw literal,
// so the value is right on every backend: desktop minifb packs pixels as
// 0xAARRGGBB, but Android packs them as 0xAABBGGRR. theme_proposal.md's
// 0x00RRGGBB literals already match the desktop layout, so nothing is swapped
// here; re-encoding also fills in the alpha they leave at zero.
static bool parse_color(const char *text, uint32_t *out) {
	if (strncmp(text, "0x", 2) == 0 || strncmp(text, "0X", 2) == 0) {
		text += 2;
	}

	size_t length = strlen(text);
	if (length != 6 && length != 8) {
		return false;
	}
	for (size_t i = 0; i < length; i++) {
		if (!isxdigit((unsigned char) text[i])) {
			return false;
		}
	}

	unsigned long value = strtoul(text, NULL, 16);
	uint32_t rgb = (uint32_t) (value & 0xFFFFFFUL);
	*out = MFB_RGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
	return true;
}

static bool parse_bool(const char *text, bool *out) {
	if (strcasecmp(text, "on") == 0 || strcasecmp(text, "true") == 0 ||
	    strcasecmp(text, "yes") == 0 || strcmp(text, "1") == 0) {
		*out = true;
		return true;
	}
	if (strcasecmp(text, "off") == 0 || strcasecmp(text, "false") == 0 ||
	    strcasecmp(text, "no") == 0 || strcmp(text, "0") == 0) {
		*out = false;
		return true;
	}
	return false;
}

static Theme *find_theme(Config *cfg, const char *name) {
	for (int i = 0; i < cfg->theme_count; i++) {
		if (strcmp(cfg->themes[i].name, name) == 0) {
			return &cfg->themes[i];
		}
	}
	return NULL;
}

static Theme *add_theme(Config *cfg, const char *name) {
	if (cfg->theme_count >= CONFIG_MAX_THEMES) {
		return NULL;
	}

	Theme *theme = &cfg->themes[cfg->theme_count++];
	snprintf(theme->name, sizeof(theme->name), "%s", name);
	theme->foreground = DEFAULT_FOREGROUND;
	theme->background = DEFAULT_BACKGROUND;
	return theme;
}

static void set_section(char *section, char *text, const char *path, int lineno) {
	char *close = strchr(text, ']');
	if (!close) {
		fprintf(stderr, "%s:%d: unterminated section header '%s'.\n", path, lineno, text);
		section[0] = '\0';
		return;
	}
	*close = '\0';

	char *name = trim(text + 1);
	if (*name == '\0' || strlen(name) >= CONFIG_THEME_NAME_MAX) {
		fprintf(stderr, "%s:%d: bad section name '%s'.\n", path, lineno, name);
		section[0] = '\0';
		return;
	}

	snprintf(section, CONFIG_THEME_NAME_MAX, "%s", name);
}

static void set_global(Config *cfg, const char *key, const char *value, const char *path, int lineno) {
	if (strcmp(key, "theme") == 0) {
		snprintf(cfg->selected, sizeof(cfg->selected), "%s", value);
		return;
	}

	bool *flag = NULL;
	if (strcmp(key, "scanlines") == 0) {
		flag = &cfg->scanlines;
	} else if (strcmp(key, "vignette") == 0) {
		flag = &cfg->vignette;
	} else {
		fprintf(stderr, "%s:%d: unknown setting '%s'.\n", path, lineno, key);
		return;
	}

	if (!parse_bool(value, flag)) {
		fprintf(stderr, "%s:%d: %s: expected on or off, got '%s'.\n", path, lineno, key, value);
	}
}

static void set_theme_color(Config *cfg, char *section, const char *key, const char *value,
                            const char *path, int lineno) {
	Theme *theme = find_theme(cfg, section);
	if (!theme) {
		theme = add_theme(cfg, section);
	}
	if (!theme) {
		fprintf(stderr, "%s:%d: too many themes, ignoring [%s].\n", path, lineno, section);
		return;
	}

	uint32_t *target = NULL;
	if (strcmp(key, "foreground") == 0) {
		target = &theme->foreground;
	} else if (strcmp(key, "background") == 0) {
		target = &theme->background;
	} else {
		fprintf(stderr, "%s:%d: [%s] unknown setting '%s'.\n", path, lineno, section, key);
		return;
	}

	if (*value == '\0') {
		fprintf(stderr, "%s:%d: [%s] %s: missing a value ('#' starts a comment).\n",
		        path, lineno, section, key);
		return;
	}

	if (!parse_color(value, target)) {
		fprintf(stderr, "%s:%d: [%s] %s: expected RRGGBB or 0xRRGGBB hex, got '%s'.\n",
		        path, lineno, section, key, value);
	}
}

// Parses one line in place; `section` carries the current [theme] name across
// calls and is empty at the top level.
static void parse_line(Config *cfg, char *line, char *section, const char *path, int lineno) {
	for (char *p = line; *p; p++) {
		if (*p == '#' || *p == ';') {
			*p = '\0';
			break;
		}
	}

	char *text = trim(line);
	if (*text == '\0') {
		return;
	}

	if (text[0] == '[') {
		set_section(section, text, path, lineno);
		return;
	}

	char *equals = strchr(text, '=');
	if (!equals) {
		fprintf(stderr, "%s:%d: expected 'key = value', got '%s'.\n", path, lineno, text);
		return;
	}
	*equals = '\0';

	char *key = trim(text);
	char *value = trim(equals + 1);

	if (section[0] == '\0') {
		set_global(cfg, key, value, path, lineno);
	} else {
		set_theme_color(cfg, section, key, value, path, lineno);
	}
}

bool config_parse_file(Config *cfg, const char *path) {
	FILE *file = fopen(path, "r");
	if (!file) {
		return false;
	}

	char section[CONFIG_THEME_NAME_MAX] = "";
	char line[512];
	int lineno = 0;

	while (fgets(line, sizeof(line), file)) {
		lineno++;
		parse_line(cfg, line, section, path, lineno);
	}
	fclose(file);

	// The selector may appear before the section it names, so resolve it once
	// the whole file has been read.
	if (cfg->selected[0] != '\0') {
		const Theme *theme = find_theme(cfg, cfg->selected);
		if (!theme) {
			fprintf(stderr, "%s: no theme named '%s'; keeping the current colours.\n",
			        path, cfg->selected);
		} else {
			cfg->foreground = theme->foreground;
			cfg->background = theme->background;
		}
	}

	return true;
}

bool config_load(Config *cfg) {
	if (config_parse_file(cfg, "toki.conf")) {
		return true;
	}

	const char *home = getenv("HOME");
	if (!home) {
		return false;
	}

	char path[1024];
	snprintf(path, sizeof(path), "%s/.config/toki/config", home);
	return config_parse_file(cfg, path);
}
