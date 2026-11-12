# Toki (時)

A lightweight, high-performance terminal and windowed clock engine written in C. Inspired by classic progress bar aesthetics and retro display terminals, **Toki** renders a real-time CPU framebuffer complete with customizable color themes, scan-lines, vignette effects, custom glyphs, and flexible scaling factors.

---

## Features

* **Multiple Rendering Backends**:
  * **Terminal Native (`toki_term.c` / Chaitea)**: Crisp text-mode rendering directly inside your terminal.
  * **Windowed Mode (`MiniFB`)**: Lightweight, hardware-accelerated window output for CPU framebuffers.
  * **Kitty Graphics Protocol** *(In Progress)*: High-resolution, pixel-accurate framebuffer rendering straight into compatible terminal emulators.
* **Retro CRT Aesthetics**: Built-in configurable scan-lines and subtle vignette effects to give your clock an authentic retro display feel.
* **Custom Glyphs & FIGlet Fonts**: Supports crisp custom bundled glyph blitting for digits and separators, alongside FIGlet font integration.
* **Dynamic Scaling**: Shrink or scale the clock face dynamically (`--scale=1`, `2`, `4`, etc., dividing base dimensions like `72/2`, `72/3`, `72/4`).
* **Configurable Themes**: Easily customize foreground and background color themes via local configuration files or sensible defaults.

---

## Project Structure

```text
├── src/
│   ├── toki.c         # Main MiniFB windowed application entry point
│   ├── toki_term.c    # Terminal-native rendering backend
│   ├── FrameBuffer.h  # CPU framebuffer management & blitting
│   ├── Glyphs.h       # Glyph definitions & custom rendering
│   └── Config.h       # Theme & configuration handler
└── README.md
```

---

## Building & Installation

Ensure you have a C99/POSIX-compliant compiler (such as `gcc` or `clang`), `make`, and the MiniFB development libraries installed.

```bash
# Clone the repository
git clone https://github.com/crux161/toki.git
cd toki

# Build the project
make
```

---

## Usage

Run Toki directly from your terminal or window server:

```bash
# Launch with default settings (scale 1)
./toki

# Launch with scale factor 2
./toki --scale=2

# View help and available options
./toki --help
```

### Command-Line Options

| Flag | Long Flag | Description |
| :--- | :--- | :--- |
| `-s` | `--scale=N` | Shrink the clock face by factor N (`1`, `2`, or `4`, default `1`) |
| `-h` | `--help` | Display help information and exit |
| `-v` | `--version` | Output version information and exit |

---

## License

Distributed under the **MIT License**. Copyright (C) 2026 Crux161.