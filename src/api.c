#include "mathjax.h"
#include "ast.h"
#include "font.h"
#include "parser.h"
#include "layout.h"
#include "render.h"
#include "kitty.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

struct mjx_ctx {
  mjx_font* font;
  mjx_font* fallback_font;
  mjx_parser* parser;
  mjx_layout_ctx* layout;
  mjx_renderer* renderer;
  mjx_opts opts;
  uint32_t fg_color;
  uint32_t bg_color;
  int display_style;
  mjx_error last_error;
};

static const char* find_fallback_font_path(const mjx_opts* opts) {
  if (opts && opts->fallback_font_path && access(opts->fallback_font_path, R_OK) == 0) {
    return opts->fallback_font_path;
  }

  const char* env = getenv("MJX_FALLBACK_FONT");
  if (env && access(env, R_OK) == 0) return env;

  const char* candidates[] = {
    "/System/Library/Fonts/PingFang.ttc",
    "/System/Library/Fonts/STHeiti Medium.ttc",
    "/System/Library/Fonts/Hiragino Sans GB.ttc",
    "/System/Library/Fonts/Supplemental/Songti.ttc",
    "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
    NULL
  };
  for (int i = 0; candidates[i]; i++) {
    if (access(candidates[i], R_OK) == 0) return candidates[i];
  }
  return NULL;
}

mjx_ctx* mjx_init(const mjx_opts* opts) {
  mjx_ctx* ctx = (mjx_ctx*)calloc(1, sizeof(mjx_ctx));
  if (!ctx) return NULL;

  /* Set defaults */
  ctx->opts.font_path = opts ? opts->font_path : NULL;
  ctx->opts.fallback_font_path = opts ? opts->fallback_font_path : NULL;
  ctx->opts.font_size = opts ? opts->font_size : 48.0;
  ctx->opts.fg_color = opts ? opts->fg_color : 0x000000FF;
  ctx->opts.bg_color = opts ? opts->bg_color : 0x00000000;
  ctx->opts.dpi = opts ? opts->dpi : 72;
  ctx->fg_color = ctx->opts.fg_color;
  ctx->bg_color = ctx->opts.bg_color;
  ctx->display_style = 1;

  /* Load font */
  if (ctx->opts.font_path) {
    ctx->font = mjx_font_load(ctx->opts.font_path, ctx->opts.font_size, ctx->opts.dpi);
    if (!ctx->font) {
      free(ctx);
      return NULL;
    }
    /* Load MATH constants */
    mjx_font_load_math_constants(ctx->font, &ctx->font->math_constants);

    const char* fallback_path = find_fallback_font_path(opts);
    if (fallback_path && strcmp(fallback_path, ctx->opts.font_path) != 0) {
      ctx->fallback_font = mjx_font_load(fallback_path, ctx->opts.font_size, ctx->opts.dpi);
    }
  }

  /* Create parser */
  mjx_parse_opts popts;
  memset(&popts, 0, sizeof(popts));
  popts.factory = mjx_factory_create();
  popts.operators = mjx_default_operators(&popts.operator_count);
  popts.macros = mjx_default_macros(&popts.macro_count);
  ctx->parser = mjx_parser_create(&popts);
  mjx_parser_init_environments(ctx->parser);

  /* Create layout context */
  if (ctx->font) {
    ctx->layout = mjx_layout_ctx_create(ctx->font);
    ctx->layout->fallback_font = ctx->fallback_font;
  }

  /* Create renderer */
  if (ctx->font) {
    ctx->renderer = mjx_renderer_create(ctx->font);
    ctx->renderer->fallback_font = ctx->fallback_font;
    ctx->renderer->fg_color = ctx->fg_color;
    ctx->renderer->bg_color = ctx->bg_color;
  }

  ctx->last_error = MJX_OK;
  return ctx;
}

void mjx_free(mjx_ctx* ctx) {
  if (!ctx) return;
  if (ctx->renderer) mjx_renderer_destroy(ctx->renderer);
  if (ctx->layout) mjx_layout_ctx_destroy(ctx->layout);
  if (ctx->parser) mjx_parser_destroy(ctx->parser);
  if (ctx->fallback_font) mjx_font_unload(ctx->fallback_font);
  if (ctx->font) mjx_font_unload(ctx->font);
  free(ctx);
}

mjx_node* mjx_parse_latex(mjx_ctx* ctx, const char* latex, mjx_style style) {
  if (!ctx || !latex) {
    ctx->last_error = MJX_ERR_INVALID_ARG;
    return NULL;
  }
  int display = (style == MJX_STYLE_DISPLAY);
  ctx->display_style = display;
  return mjx_parser_parse_latex(ctx->parser, latex, display);
}

mjx_node* mjx_parse_mathml(mjx_ctx* ctx, const char* xml, mjx_style style) {
  if (!ctx || !xml) {
    ctx->last_error = MJX_ERR_INVALID_ARG;
    return NULL;
  }
  int display = (style == MJX_STYLE_DISPLAY);
  ctx->display_style = display;
  return mjx_parser_parse_mathml(ctx->parser, xml, display);
}

const char* mjx_node_type_name(const mjx_node* node) {
  return mjx_node_type_str(node);
}

mjx_node* mjx_node_child(const mjx_node* node, size_t idx) {
  if (!node || idx >= node->child_count) return NULL;
  return node->children[idx];
}

size_t mjx_node_child_count(const mjx_node* node) {
  if (!node) return 0;
  return node->child_count;
}

const char* mjx_node_text(const mjx_node* node) {
  if (!node) return NULL;
  return node->text;
}

mjx_box* mjx_layout(mjx_ctx* ctx, mjx_node* node) {
  if (!ctx || !node) {
    ctx->last_error = MJX_ERR_INVALID_ARG;
    return NULL;
  }
  return mjx_layout_node(ctx->layout, node, ctx->display_style);
}

void mjx_box_free(mjx_box* box) {
  mjx_box_destroy(box);
}

double mjx_box_width(const mjx_box* box) {
  return box ? box->width : 0;
}

double mjx_box_height(const mjx_box* box) {
  return box ? box->height : 0;
}

double mjx_box_depth(const mjx_box* box) {
  return box ? box->depth : 0;
}

mjx_buf* mjx_render(mjx_ctx* ctx, mjx_box* box) {
  if (!ctx || !box) {
    ctx->last_error = MJX_ERR_INVALID_ARG;
    return NULL;
  }
  return mjx_render_box(ctx->renderer, box);
}



unsigned int mjx_buf_width(const mjx_buf* buf) {
  return buf ? buf->width : 0;
}

unsigned int mjx_buf_height(const mjx_buf* buf) {
  return buf ? buf->height : 0;
}

const uint32_t* mjx_buf_pixels(const mjx_buf* buf) {
  return buf ? buf->pixels : NULL;
}

mjx_error mjx_display_kitty(const mjx_buf* buf, const mjx_kitty_opts* opts) {
  if (!buf) return MJX_ERR_INVALID_ARG;
  mjx_kitty_opts default_opts;
  if (!opts) {
    memset(&default_opts, 0, sizeof(default_opts));
    default_opts.placement = MJX_KITTY_CURSOR;
    opts = &default_opts;
  }
  return mjx_kitty_display(buf, opts);
}

mjx_buf* mjx_render_latex(mjx_ctx* ctx, const char* latex, mjx_style style) {
  if (!ctx || !latex) {
    ctx->last_error = MJX_ERR_INVALID_ARG;
    return NULL;
  }

  mjx_node* node = mjx_parse_latex(ctx, latex, style);
  if (!node) {
    ctx->last_error = MJX_ERR_PARSE;
    return NULL;
  }

  mjx_box* box = mjx_layout(ctx, node);
  if (!box) {
    ctx->last_error = MJX_ERR_LAYOUT;
    return NULL;
  }

  mjx_buf* buf = mjx_render(ctx, box);
  mjx_box_free(box);

  return buf;
}

mjx_error mjx_last_error(const mjx_ctx* ctx) {
  return ctx ? ctx->last_error : MJX_ERR_INVALID_ARG;
}

const char* mjx_error_string(mjx_error err) {
  switch (err) {
    case MJX_OK: return "success";
    case MJX_ERR_NOMEM: return "out of memory";
    case MJX_ERR_FONT: return "font error";
    case MJX_ERR_PARSE: return "parse error";
    case MJX_ERR_LAYOUT: return "layout error";
    case MJX_ERR_RENDER: return "render error";
    case MJX_ERR_KITTY: return "kitty protocol error";
    case MJX_ERR_INVALID_ARG: return "invalid argument";
    case MJX_ERR_UNSUPPORTED: return "unsupported operation";
    default: return "unknown error";
  }
}
