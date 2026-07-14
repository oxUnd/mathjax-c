#ifndef MJX_LAYOUT_H
#define MJX_LAYOUT_H

#include "ast.h"
#include "font.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Box types */
typedef enum {
  MJX_BOX_HBOX,     /* horizontal box */
  MJX_BOX_VBOX,     /* vertical box */
  MJX_BOX_GLYPH,    /* single glyph */
  MJX_BOX_RULE,     /* horizontal/vertical rule (line) */
  MJX_BOX_GLUE,     /* glue (space with stretch/shrink) */
  MJX_BOX_ATOM,     /* TeX atom (contains TeX class info) */
  MJX_BOX_DIAGONAL, /* diagonal line (for \cancel etc.) */
} mjx_box_type;

/* A positioned child in a box */
typedef struct mjx_box_child {
  struct mjx_box* box;
  double x;          /* horizontal offset */
  double y;          /* vertical offset (from baseline) */
  struct mjx_box_child* next;
} mjx_box_child;

/* The box tree node */
typedef struct mjx_box {
  mjx_box_type type;
  double width;
  double height;
  double depth;
  double shift;         /* shift amount for VBox children */
  mjx_texclass tex_class;

  /* Children (for HBOX/VBOX) */
  mjx_box_child* children;

  /* For GLYPH boxes */
  uint32_t codepoint;
  unsigned int glyph_id;
  int font_index;
  double italic_correction;
  double font_size;
  int allow_nonuniform_scale;

  /* For RULE boxes */
  double rule_thickness;
  int is_vertical_rule;

  /* For DIAGONAL boxes */
  double diag_x1, diag_y1, diag_x2, diag_y2;
  double diag_thickness;

  /* For GLUE boxes */
  double stretch;
  double shrink;

  /* Bounding box in final rendered space */
  double bb_x;
  double bb_y;

  /* Linked list for pooled allocation */
  struct mjx_box* next_pool;
} mjx_box;

/* Layout context */
typedef struct mjx_layout_ctx {
  mjx_font* font;
  mjx_font* fallback_font;
  mjx_math_constants* mc;
  double base_font_size;
  double font_size;
  double script_size_multiplier;
  double script_min_size;
  double mu_unit;     /* 1mu = 1/18 em */
  int script_depth;   /* nonzero while laying out scripts/limits */
} mjx_layout_ctx;

/* Create layout context */
mjx_layout_ctx* mjx_layout_ctx_create(mjx_font* font);
void            mjx_layout_ctx_destroy(mjx_layout_ctx* ctx);

/* Main layout function: MathML node → box tree */
mjx_box* mjx_layout_node(mjx_layout_ctx* ctx, mjx_node* node, int display);

/* Layout a row of children (mrow handler) */
mjx_box* mjx_layout_row(mjx_layout_ctx* ctx, mjx_node** children, size_t count, int display);

/* Box management */
mjx_box* mjx_box_create(mjx_box_type type);
mjx_box* mjx_box_create_glyph(mjx_layout_ctx* ctx, uint32_t codepoint);
mjx_box* mjx_box_create_rule(double width, double height, double depth);
mjx_box* mjx_box_create_glue(double width, double stretch, double shrink);
void     mjx_box_add_child(mjx_box* parent, mjx_box* child, double x, double y);
void     mjx_box_destroy(mjx_box* box);

/* TeX spacing between two adjacent boxes */
double mjx_tex_spacing(mjx_texclass prev, mjx_texclass cur, int script_level);

/* Internal layout functions */
mjx_box* mjx_layout_scripts(mjx_layout_ctx* ctx, mjx_node* node, int display);
mjx_box* mjx_layout_fraction(mjx_layout_ctx* ctx, mjx_node* node, int display);
mjx_box* mjx_layout_sqrt(mjx_layout_ctx* ctx, mjx_node* node, int display);
mjx_box* mjx_layout_root(mjx_layout_ctx* ctx, mjx_node* node, int display);
mjx_box* mjx_layout_delimited(mjx_layout_ctx* ctx, mjx_box* inner,
                              const char* left_delim, const char* right_delim,
                              int fixed_size);
mjx_box* mjx_layout_delimited_matrix(mjx_layout_ctx* ctx, mjx_node* table,
                                     const char* left_delim, const char* right_delim,
                                     int display);
mjx_box* mjx_box_create_atom(mjx_box* inner, mjx_texclass tex_class);
void     mjx_box_hpack(mjx_box* hbox);

/* Style helpers */
double mjx_current_font_size(mjx_layout_ctx* ctx, int script_level);
int    mjx_is_display_style(int display, int script_level);
void   mjx_layout_apply_styles(mjx_node* node, int display);

#ifdef __cplusplus
}
#endif

#endif /* MJX_LAYOUT_H */
