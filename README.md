# mathjax-c

`mathjax-c` is an experimental C renderer for TeX/LaTeX math expressions, inspired by MathJax's TeX parsing and layout behavior. It parses a useful subset of LaTeX math, lays it out with OpenType math font metrics, and renders the result to an RGBA bitmap or to the Kitty graphics protocol.

The project is still a work in progress. The current focus is visual compatibility with MathJax for common mathematical notation, including scripts, fractions, radicals, delimiters, matrices, operators, extensible arrows, accents, boxes, and many standard TeX symbols.

## Features

- C11 library API for parse, layout, and render.
- CLI renderer for terminal preview or PNG output.
- STIX Two Math based glyph metrics and rendering.
- Support for many common LaTeX math constructs:
  - Greek letters and symbol commands
  - superscripts and subscripts
  - fractions and binomial-style constructs
  - square roots and indexed roots
  - stretchy delimiters
  - large operators with limits and nolimits
  - matrices and array-like environments
  - accents, wide accents, arrows, and boxed expressions
- Regression test suite covering the currently tracked command set.
- Market/common formula diff gallery under `test/diff/`.

## Dependencies

- CMake 3.16+
- C11 compiler
- FreeType 2
- HarfBuzz
- STIX Two Math font

On macOS with Homebrew:

```sh
brew install cmake freetype harfbuzz
```

The repository includes `fonts/STIXTwoMath-Regular.ttf` for local testing.

## Build

```sh
cmake -S . -B build
cmake --build build
```

This builds:

- `build/libmathjax.a`
- `build/mathjax-cli`

## CLI Usage

Render to the Kitty graphics protocol:

```sh
./build/mathjax-cli '\frac{-b\pm\sqrt{b^2-4ac}}{2a}'
```

Render to PNG:

```sh
./build/mathjax-cli \
  -f fonts/STIXTwoMath-Regular.ttf \
  -o /tmp/formula.png \
  '\sqrt{x^2+y^2}'
```

Useful options:

```text
-f, --font PATH       STIX Two Math font path
-s, --size SIZE       font size in points
-d, --dpi DPI         render DPI
-c, --color RRGGBB    foreground color
-b, --bg RRGGBB       background color
-i, --inline          inline math style
--dump-ast            print parsed AST
--dump-box-tree       print layout box summary
-o, --output FILE     write PNG output
```

## Tests

Run the full regression suite:

```sh
bash tools/test.sh
```

Run the suite against a custom CLI build:

```sh
CLI=./build-asan/mathjax-cli bash tools/test.sh
```

## Sanitizer Check

AddressSanitizer and UndefinedBehaviorSanitizer can be enabled with a separate build directory:

```sh
cmake -S . -B build-asan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined' \
  -DCMAKE_SHARED_LINKER_FLAGS='-fsanitize=address,undefined'

cmake --build build-asan
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 \
CLI=./build-asan/mathjax-cli \
bash tools/test.sh
```

On macOS, `detect_leaks=1` may not be supported by the system ASan runtime.

## Diff Gallery

Generate the common formula gallery:

```sh
JOBS=10 node scripts/generate_market_gallery.js
```

Open:

```text
test/diff/index.html
```

The generated PNG assets are written to `test/diff/assets/` and are intentionally ignored by git.

## Library API

The public API is declared in `include/mathjax.h`.

Typical flow:

```c
mjx_opts opts = {
  .font_path = "fonts/STIXTwoMath-Regular.ttf",
  .font_size = 48.0,
  .fg_color = 0x000000FF,
  .bg_color = 0xFFFFFFFF,
  .dpi = 72,
};

mjx_ctx* ctx = mjx_init(&opts);
mjx_buf* buf = mjx_render_latex(ctx, "\\sqrt{x^2+y^2}", MJX_STYLE_DISPLAY);

/* use mjx_buf_pixels(), mjx_buf_width(), mjx_buf_height() */

mjx_buf_free(buf);
mjx_free(ctx);
```

## Project Status

This is not a complete MathJax replacement yet. It is a focused C implementation that is being expanded formula by formula against MathJax-style output. Unsupported commands may parse as fallback identifiers or render approximately until implemented.
