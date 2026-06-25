#include "mathjax.h"
#include "kitty.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../src/third_party/stb_image_write.h"

static void print_usage(const char* prog) {
  fprintf(stderr, "Usage: %s [options] 'latex-formula'\n", prog);
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -f, --font PATH     STIX Two Math font path\n");
  fprintf(stderr, "  -s, --size SIZE     Font size in points (default: 48)\n");
  fprintf(stderr, "  -d, --dpi DPI       DPI (default: 72)\n");
  fprintf(stderr, "  -c, --color COLOR   Foreground color (RRGGBB hex, default: 000000)\n");
  fprintf(stderr, "  -b, --bg COLOR      Background color (RRGGBB hex, default: ffffff)\n");
  fprintf(stderr, "  -i, --inline        Inline style (default: display math)\n");
  fprintf(stderr, "  --dump-ast          Print AST tree to stderr and exit\n");
  fprintf(stderr, "  --dump-box-tree     Print box tree to stderr and exit\n");
  fprintf(stderr, "  -o, --output FILE   Save rendered image to PNG file\n");
  fprintf(stderr, "  -h, --help          Show this help\n");
}

static void dump_ast(const mjx_node* node, int depth) {
  if (!node) return;
  for (int i = 0; i < depth; i++) fprintf(stderr, "  ");
  fprintf(stderr, "%s", mjx_node_type_name(node));
  const char* text = mjx_node_text(node);
  if (text && text[0]) fprintf(stderr, " \"%s\"", text);
  size_t n = mjx_node_child_count(node);
  if (n > 0) fprintf(stderr, " [%zu children]", n);
  fprintf(stderr, "\n");
  for (size_t i = 0; i < n; i++) {
    dump_ast(mjx_node_child(node, i), depth + 1);
  }
}

int main(int argc, char** argv) {
  const char* font_path = NULL;
  double font_size = 48.0;
  unsigned int dpi = 72;
  uint32_t fg = 0x000000FF;
  uint32_t bg = 0xFFFFFFFF;
  int display = 1;
  const char* latex = NULL;
  const char* output_path = NULL;
  int dump_ast_flag = 0;
  int dump_box_flag = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--font") == 0) {
      if (i + 1 < argc) font_path = argv[++i];
    } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--size") == 0) {
      if (i + 1 < argc) font_size = atof(argv[++i]);
    } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dpi") == 0) {
      if (i + 1 < argc) dpi = (unsigned int)atoi(argv[++i]);
    } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--color") == 0) {
      if (i + 1 < argc) {
        unsigned long c = strtoul(argv[++i], NULL, 16);
        fg = ((c & 0xFF) << 24) | ((c & 0xFF00) << 8) | ((c & 0xFF0000) >> 8) | 0xFF;
      }
    } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--bg") == 0) {
      if (i + 1 < argc) {
        unsigned long c = strtoul(argv[++i], NULL, 16);
        bg = ((c & 0xFF) << 24) | ((c & 0xFF00) << 8) | ((c & 0xFF0000) >> 8) | 0xFF;
      }
    } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--inline") == 0) {
      display = 0;
    } else if (strcmp(argv[i], "--dump-ast") == 0) {
      dump_ast_flag = 1;
    } else if (strcmp(argv[i], "--dump-box-tree") == 0) {
      dump_box_flag = 1;
    } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
      if (i + 1 < argc) output_path = argv[++i];
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      print_usage(argv[0]);
      return 0;
    } else if (argv[i][0] != '-') {
      latex = argv[i];
    }
  }

  if (!latex) {
    print_usage(argv[0]);
    return 1;
  }

  /* Try to find font if not specified */
  if (!font_path) {
    const char* candidates[] = {
      "./fonts/STIXTwoMath-Regular.ttf",
      "../fonts/STIXTwoMath-Regular.ttf",
      "/usr/local/share/fonts/STIXTwoMath-Regular.ttf",
      NULL
    };
    for (int i = 0; candidates[i]; i++) {
      if (access(candidates[i], R_OK) == 0) {
        font_path = candidates[i];
        break;
      }
    }
  }

  if (!font_path) {
    fprintf(stderr, "Error: STIX Two Math font not found.\n");
    fprintf(stderr, "Download it and specify with -f or place at ./fonts/STIXTwoMath-Regular.ttf\n");
    return 1;
  }

  mjx_opts opts;
  memset(&opts, 0, sizeof(opts));
  opts.font_path = font_path;
  opts.font_size = font_size;
  opts.fg_color = fg;
  opts.bg_color = bg;
  opts.dpi = dpi;

  mjx_ctx* ctx = mjx_init(&opts);
  if (!ctx) {
    fprintf(stderr, "Error: Failed to initialize MathJax-C\n");
    return 1;
  }

  mjx_style style = display ? MJX_STYLE_DISPLAY : MJX_STYLE_INLINE;

  if (dump_ast_flag) {
    mjx_node* node = mjx_parse_latex(ctx, latex, style);
    if (!node) {
      fprintf(stderr, "Error: parse failed\n");
      mjx_free(ctx);
      return 1;
    }
    dump_ast(node, 0);
    mjx_free(ctx);
    return 0;
  }

  mjx_node* node = mjx_parse_latex(ctx, latex, style);
  if (!node) {
    fprintf(stderr, "Error: parse failed\n");
    mjx_free(ctx);
    return 1;
  }

  mjx_box* box = mjx_layout(ctx, node);
  if (!box) {
    fprintf(stderr, "Error: layout failed\n");
    mjx_free(ctx);
    return 1;
  }

  if (dump_box_flag) {
    fprintf(stderr, "Box: w=%.1f h=%.1f d=%.1f\n",
            mjx_box_width(box), mjx_box_height(box), mjx_box_depth(box));
    mjx_box_free(box);
    mjx_free(ctx);
    return 0;
  }

  mjx_buf* buf = mjx_render(ctx, box);
  mjx_box_free(box);

  if (!buf) {
    fprintf(stderr, "Error: render failed\n");
    mjx_free(ctx);
    return 1;
  }

  if (output_path) {
    unsigned int w = mjx_buf_width(buf);
    unsigned int h = mjx_buf_height(buf);
    const uint32_t* pixels = mjx_buf_pixels(buf);
    if (!pixels || w == 0 || h == 0) {
      fprintf(stderr, "Error: empty rendered buffer\n");
      mjx_buf_free(buf);
      mjx_free(ctx);
      return 1;
    }
    unsigned char* png_data = (unsigned char*)malloc(w * h * 4);
    if (!png_data) { mjx_buf_free(buf); mjx_free(ctx); return 1; }
    for (unsigned int y = 0; y < h; y++) {
      for (unsigned int x = 0; x < w; x++) {
        uint32_t p = pixels[y * w + x];
        unsigned char* dst = png_data + (y * w + x) * 4;
        dst[0] = (p >> 24) & 0xFF;
        dst[1] = (p >> 16) & 0xFF;
        dst[2] = (p >> 8) & 0xFF;
        dst[3] = p & 0xFF;
      }
    }
    int ok = stbi_write_png(output_path, (int)w, (int)h, 4, png_data, (int)w * 4);
    free(png_data);
    if (!ok) {
      fprintf(stderr, "Error: failed to write PNG to %s\n", output_path);
      mjx_buf_free(buf);
      mjx_free(ctx);
      return 1;
    }
    fprintf(stderr, "Wrote %ux%u PNG to %s\n", w, h, output_path);
  } else {
    mjx_kitty_opts kopts;
    memset(&kopts, 0, sizeof(kopts));
    kopts.placement = MJX_KITTY_CURSOR;
    mjx_error err = mjx_display_kitty(buf, &kopts);
    if (err != MJX_OK) {
      fprintf(stderr, "Error displaying via Kitty protocol: %s\n", mjx_error_string(err));
    }
  }

  mjx_buf_free(buf);
  mjx_free(ctx);
  return 0;
}