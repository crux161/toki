#!/usr/bin/env python3
"""Slice toki-font.png into individual digit images and generate a C header

for direct UEFI GOP framebuffer rendering.
"""

from pathlib import Path
from PIL import Image

# Configuration
SPRITE_SHEET = "toki-font.png"
COLS = 5
ROWS = 2

# Grid mapping for digits 0-9 across the 2 rows
DIGIT_GRID = [
    ('0', 0, 0), ('1', 1, 0), ('2', 2, 0), ('3', 3, 0), ('4', 4, 0),
    ('5', 0, 1), ('6', 1, 1), ('7', 2, 1), ('8', 3, 1), ('9', 4, 1)
]

def main():
    if not Path(SPRITE_SHEET).exists():
        print(f"Error: Could not find {SPRITE_SHEET}")
        return

    sheet = Image.open(SPRITE_SHEET).convert("RGBA")
    sheet_w, sheet_h = sheet.size
    
    cell_w = sheet_w // COLS
    cell_h = sheet_h // ROWS
    print(f"Detected sprite sheet: {sheet_w}x{sheet_h}px | Cell size: {cell_w}x{cell_h}px")

    # Step 1: Export individual image files
    #output_dir = Path("exported_glyphs")
    #output_dir.mkdir(exist_ok=True)
    output_dir = Path("exported_glyphs")
    glyph_data = {}

    for char_val, c, r in DIGIT_GRID:
        box = (c * cell_w, r * cell_h, (c + 1) * cell_w, (r + 1) * cell_h)
        glyph_img = sheet.crop(box)
        
        # Save individual sprite
        out_path = output_dir / f"digit_{char_val}.png"
        glyph_img.save(out_path)
        
        # Extract pixel mask for C header (1 = foreground, 0 = transparent)
        pixels = []
        for y in range(cell_h):
            row_bytes = []
            for x in range(cell_w):
                # Check alpha or brightness (assuming white text on black/transparent background)
                r_val, g_val, b_val, a_val = glyph_img.getpixel((x, y))
                is_set = 1 if (r_val > 128 or a_val > 128) else 0
                row_bytes.append(is_set)
            pixels.append(row_bytes)
            
        glyph_data[char_val] = pixels

    print(f"Saved 10 individual sprites to {output_dir}/")

    # Step 2: Generate C Header file for GOP framebuffer
    c_header_path = Path("gop_font.h")
    with open(c_header_path, "w") as f:
        f.write("/* Auto-generated GOP Font Header */\n")
        f.write("#ifndef GOP_FONT_H\n#define GOP_FONT_H\n\n")
        f.write(f"#define FONT_WIDTH  {cell_w}\n")
        f.write(f"#define FONT_HEIGHT {cell_h}\n\n")
        
        for char_val, pixels in glyph_data.items():
            name = f"DIGIT_{char_val}"
            f.write(f"static const uint8_t {name}[{cell_h}][{cell_w}] = {{\n")
            for row in pixels:
                f.write("    { " + ", ".join(map(str, row)) + " },\n")
            f.write("};\n\n")
            
        f.write("#endif // GOP_FONT_H\n")

    print(f"Generated C header -> {c_header_path}")

if __name__ == "__main__":
    main()
