#ifndef MATHJAX_C_H
#define MATHJAX_C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque context handles */
typedef struct mjx_ctx mjx_ctx;
typedef struct mjx_node mjx_node;
typedef struct mjx_box mjx_box;
typedef struct mjx_buf mjx_buf;
typedef struct mjx_font mjx_font;

/* Error codes */
typedef enum {
  MJX_OK = 0,
  MJX_ERR_NOMEM,
  MJX_ERR_FONT,
  MJX_ERR_PARSE,
  MJX_ERR_LAYOUT,
  MJX_ERR_RENDER,
  MJX_ERR_KITTY,
  MJX_ERR_INVALID_ARG,
  MJX_ERR_UNSUPPORTED,
} mjx_error;

/* Math style */
typedef enum {
  MJX_STYLE_DISPLAY,   /* displaystyle ($$...$$) */
  MJX_STYLE_INLINE,    /* inline style ($...$) */
  MJX_STYLE_SCRIPT,    /* script style (first-level sup/sub) */
  MJX_STYLE_SCRIPTSCRIPT /* scriptscript style */
} mjx_style;

/* Alignment */
typedef enum {
  MJX_ALIGN_LEFT,
  MJX_ALIGN_CENTER,
  MJX_ALIGN_RIGHT,
} mjx_align;

/* Kitty image placement */
typedef enum {
  MJX_KITTY_CURSOR,  /* place at cursor position */
  MJX_KITTY_CENTER,  /* center on screen */
  MJX_KITTY_ANCHOR   /* anchor to specific row/col */
} mjx_kitty_placement;

/* Kitty protocol options */
typedef struct {
  mjx_kitty_placement placement;
  unsigned int col;
  unsigned int row;
  unsigned int width;
  unsigned int height;
  int stretch;         /* nonzero to stretch */
} mjx_kitty_opts;

/* Options for configuration */
typedef struct {
  const char* font_path;      /* path to STIX Two Math font */
  double font_size;           /* base font size in points */
  uint32_t fg_color;          /* foreground color (0xRRGGBB) */
  uint32_t bg_color;          /* background color (0xRRGGBB, 0=transparent) */
  unsigned int dpi;           /* dots per inch for font scaling */
} mjx_opts;

/* Init / Teardown */
mjx_ctx* mjx_init(const mjx_opts* opts);
void     mjx_free(mjx_ctx* ctx);

/* Parsing */
mjx_node* mjx_parse_latex(mjx_ctx* ctx, const char* latex, mjx_style style);
mjx_node* mjx_parse_mathml(mjx_ctx* ctx, const char* xml, mjx_style style);

/* Tree inspection */
const char* mjx_node_type_name(const mjx_node* node);
mjx_node*   mjx_node_child(const mjx_node* node, size_t idx);
size_t      mjx_node_child_count(const mjx_node* node);
const char* mjx_node_text(const mjx_node* node);

/* Layout */
mjx_box* mjx_layout(mjx_ctx* ctx, mjx_node* node);
void     mjx_box_free(mjx_box* box);
double   mjx_box_width(const mjx_box* box);
double   mjx_box_height(const mjx_box* box);
double   mjx_box_depth(const mjx_box* box);

/* Rendering */
mjx_buf* mjx_render(mjx_ctx* ctx, mjx_box* box);
void     mjx_buf_free(mjx_buf* buf);
unsigned int mjx_buf_width(const mjx_buf* buf);
unsigned int mjx_buf_height(const mjx_buf* buf);
const uint32_t* mjx_buf_pixels(const mjx_buf* buf);

/* Kitty protocol display */
mjx_error mjx_display_kitty(const mjx_buf* buf, const mjx_kitty_opts* opts);

/* High-level convenience: parse + layout + render */
mjx_buf* mjx_render_latex(mjx_ctx* ctx, const char* latex, mjx_style style);

/* Error handling */
mjx_error mjx_last_error(const mjx_ctx* ctx);
const char* mjx_error_string(mjx_error err);

#ifdef __cplusplus
}
#endif

#endif /* MATHJAX_C_H */
