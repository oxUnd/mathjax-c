#!/bin/bash
set -e

echo "=== mathjax-c setup ==="
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BASE_DIR="$SCRIPT_DIR/.."
FONTS_DIR="$BASE_DIR/fonts"
THIRD_PARTY_DIR="$BASE_DIR/src/third_party"

mkdir -p "$FONTS_DIR" "$THIRD_PARTY_DIR"

# Download STIX Two Math font
echo "Downloading STIX Two Math font..."
if [ ! -f "$FONTS_DIR/STIXTwoMath-Regular.ttf" ]; then
  if command -v curl &> /dev/null; then
    curl -L -o "$FONTS_DIR/STIXTwoMath-Regular.ttf" \
      "https://github.com/google/fonts/raw/main/ofl/stixtwomath/STIXTwoMath-Regular.ttf"
  elif command -v wget &> /dev/null; then
    wget -O "$FONTS_DIR/STIXTwoMath-Regular.ttf" \
      "https://github.com/google/fonts/raw/main/ofl/stixtwomath/STIXTwoMath-Regular.ttf"
  fi
  echo "STIX Two Math font downloaded."
else
  echo "STIX Two Math font already exists."
fi

# Download stb_image_write.h (single header library for PNG encoding)
echo "Downloading stb_image_write.h..."
if [ ! -f "$THIRD_PARTY_DIR/stb_image_write.h" ]; then
  if command -v curl &> /dev/null; then
    curl -L -o "$THIRD_PARTY_DIR/stb_image_write.h" \
      "https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h"
  elif command -v wget &> /dev/null; then
    wget -O "$THIRD_PARTY_DIR/stb_image_write.h" \
      "https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h"
  fi
  echo "stb_image_write.h downloaded."
else
  echo "stb_image_write.h already exists."
fi

# Download uthash.h (hash table)
echo "Downloading uthash.h..."
if [ ! -f "$THIRD_PARTY_DIR/uthash.h" ]; then
  if command -v curl &> /dev/null; then
    curl -L -o "$THIRD_PARTY_DIR/uthash.h" \
      "https://raw.githubusercontent.com/troydhanson/uthash/master/src/uthash.h"
  elif command -v wget &> /dev/null; then
    wget -O "$THIRD_PARTY_DIR/uthash.h" \
      "https://raw.githubusercontent.com/troydhanson/uthash/master/src/uthash.h"
  fi
  echo "uthash.h downloaded."
else
  echo "uthash.h already exists."
fi

echo "Checking system dependencies..."
# Check for freetype2
if command -v pkg-config &> /dev/null; then
  if pkg-config --exists freetype2; then
    echo "  ✓ FreeType found"
  else
    echo "  ✗ FreeType not found via pkg-config"
    echo "    Install: brew install freetype (macOS) or apt install libfreetype6-dev (Linux)"
  fi
  if pkg-config --exists harfbuzz; then
    echo "  ✓ HarfBuzz found"
  else
    echo "  ✗ HarfBuzz not found via pkg-config"
    echo "    Install: brew install harfbuzz (macOS) or apt install libharfbuzz-dev (Linux)"
  fi
else
  echo "  ⚠ pkg-config not found"
  echo "    Install: brew install pkg-config (macOS) or apt install pkg-config (Linux)"
fi

echo ""
echo "=== Setup complete ==="
echo "Now run:"
echo "  mkdir -p build && cd build && cmake .. && make"
echo "  ./mathjax-cli -f fonts/STIXTwoMath-Regular.ttf '\\frac{1}{2}'"
