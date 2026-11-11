#!/bin/sh

# show all fonts should look like out put of
FIGLET_FONTS_DIR="/opt/homebrew/Cellar/figlet/2.2.5/share/figlet/fonts"
for f in $FIGLET_FONTS_DIR/*.flf; do
    echo "\n$f"; # we want to match file name to preview with whole path etc
    figlet -f "$f" 0123456789%;
done
