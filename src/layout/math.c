#include "layout.h"
#include "font.h"
#include "font_data.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

mjx_layout_ctx* mjx_layout_ctx_create(mjx_font* font) {
  mjx_layout_ctx* ctx = (mjx_layout_ctx*)calloc(1, sizeof(mjx_layout_ctx));
  if (!ctx) return NULL;
  ctx->font = font;
  ctx->fallback_font = NULL;
  ctx->base_font_size = font->em_size;
  ctx->font_size = ctx->base_font_size;
  ctx->script_size_multiplier = 1.0 / sqrt(2.0);
  ctx->script_min_size = 0.4 * ctx->base_font_size;
  ctx->mu_unit = ctx->font_size / 18.0;
  ctx->mc = &font->math_constants;
  return ctx;
}

void mjx_layout_ctx_destroy(mjx_layout_ctx* ctx) {
  free(ctx);
}

double mjx_current_font_size(mjx_layout_ctx* ctx, int script_level) {
  double size = ctx->base_font_size > 0.0 ? ctx->base_font_size : ctx->font_size;
  int level = script_level;
  if (level < 0) level = 0;
  if (level > 2) level = 2;
  if (level > 0) {
    size *= pow(ctx->script_size_multiplier > 0.0 ? ctx->script_size_multiplier : 1.0 / sqrt(2.0), level);
    if (size < ctx->script_min_size) size = ctx->script_min_size;
  }
  return size;
}

int mjx_is_display_style(int display, int script_level) {
  if (script_level > 0) return 0;
  return display;
}

static int is_accent_node(mjx_node* node);
static int is_under_accent_node(mjx_node* node);

static int attr_bool(const char* value, int fallback) {
  if (!value) return fallback;
  if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) return 1;
  if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) return 0;
  return fallback;
}

static int attr_int(const char* value, int fallback) {
  if (!value || !value[0]) return fallback;
  return atoi(value);
}

static int node_is_accent_child(mjx_node* node, const char* attr_name) {
  const char* explicit_attr = mjx_node_get_attr(node, attr_name);
  if (explicit_attr) return attr_bool(explicit_attr, 0);
  if (!node || node->type != MJX_NODE_MO || !node->text) return 0;
  return is_accent_node(node) || is_under_accent_node(node);
}

static void apply_styles_rec(mjx_node* node, int display, int level, int prime);

static void apply_child_styles(mjx_node* node, int display, int level, int prime) {
  if (!node) return;

  switch (node->type) {
    case MJX_NODE_MFRAC: {
      int child_level = level;
      if (!display || level > 0) child_level++;
      if (node->child_count > 0) apply_styles_rec(node->children[0], 0, child_level, prime);
      if (node->child_count > 1) apply_styles_rec(node->children[1], 0, child_level, 1);
      return;
    }

    case MJX_NODE_MSUB:
    case MJX_NODE_MSUP:
    case MJX_NODE_MSUBSUP:
    case MJX_NODE_MMULTISCRIPTS:
      if (node->child_count > 0) apply_styles_rec(node->children[0], display, level, prime);
      for (size_t i = 1; i < node->child_count; i++) {
        int child_prime = prime || (i == 1 && node->type != MJX_NODE_MSUP);
        apply_styles_rec(node->children[i], 0, level + 1, child_prime);
      }
      return;

    case MJX_NODE_MROOT:
      if (node->child_count > 0) apply_styles_rec(node->children[0], display, level, 1);
      if (node->child_count > 1) apply_styles_rec(node->children[1], 0, level + 2, prime);
      return;

    case MJX_NODE_MUNDER:
    case MJX_NODE_MOVER:
    case MJX_NODE_MUNDEROVER: {
      if (node->child_count > 0) {
        int has_over = (node->type == MJX_NODE_MOVER || node->type == MJX_NODE_MUNDEROVER);
        apply_styles_rec(node->children[0], display, level, prime || has_over);
      }
      for (size_t i = 1; i < node->child_count; i++) {
        const char* attr = (node->type == MJX_NODE_MUNDER || (node->type == MJX_NODE_MUNDEROVER && i == 1))
          ? "accentunder" : "accent";
        int child_level = node_is_accent_child(node->children[i], attr) ? level : level + 1;
        int child_prime = prime || (i == 1 && node->type != MJX_NODE_MOVER);
        apply_styles_rec(node->children[i], 0, child_level, child_prime);
      }
      return;
    }

    default:
      for (size_t i = 0; i < node->child_count; i++) {
        apply_styles_rec(node->children[i], display, level, prime);
      }
      return;
  }
}

static void apply_styles_rec(mjx_node* node, int display, int level, int prime) {
  if (!node) return;

  const char* display_attr = mjx_node_get_attr(node, "displaystyle");
  const char* level_attr = mjx_node_get_attr(node, "scriptlevel");
  if (display_attr) display = attr_bool(display_attr, display);
  if (level_attr) level = attr_int(level_attr, level);
  if (level < 0) level = 0;

  node->display_style = display;
  node->script_level = level;
  node->prime_style = prime;

  apply_child_styles(node, display, level, prime);
}

void mjx_layout_apply_styles(mjx_node* node, int display) {
  apply_styles_rec(node, display, 0, 0);
}

static double delimiter_axis_shift(mjx_layout_ctx* ctx, mjx_box* glyph, uint32_t cp);
static mjx_box* shift_operator_glyph(mjx_box* glyph, double shift);

static uint32_t math_italic_codepoint(uint32_t cp) {
  if (cp >= 'A' && cp <= 'Z') return 0x1D434 + (cp - 'A');
  if (cp >= 'a' && cp <= 'z') {
    if (cp == 'h') return 0x210E;
    return 0x1D44E + (cp - 'a');
  }
  return cp;
}

static int token_has_mathvariant(const mjx_node* node) {
  return mjx_node_get_attr(node, "mathvariant") != NULL;
}

static uint32_t layout_variant_codepoint(mjx_node* node, uint32_t cp) {
  if (!node || node->type != MJX_NODE_MI) return cp;

  if (node->variant == MJX_VARIANT_ITALIC) return math_italic_codepoint(cp);
  if (node->variant == MJX_VARIANT_NORMAL && token_has_mathvariant(node)) return cp;

  /* MathML's default for single-letter identifiers is italic.  The parser
   * generally emits TeX variables as mi tokens, so map Latin letters here
   * unless the source explicitly requested normal/roman style. */
  if (node->variant == MJX_VARIANT_NORMAL) return math_italic_codepoint(cp);
  return cp;
}

static int table_col_align(const char* align_str, size_t col, size_t ncols) {
  if (!align_str || !align_str[0]) return 0;

  if (strcmp(align_str, "right") == 0) return 2;
  if (strcmp(align_str, "center") == 0) return 1;
  if (strcmp(align_str, "left") == 0) return 0;

  if (ncols == 2 && strcmp(align_str, "rcl") == 0) {
    return col == 0 ? 2 : 0;
  }

  size_t len = strlen(align_str);
  if (len == 0) return 0;
  char a = align_str[col < len ? col : len - 1];
  if (a == 'r') return 2;
  if (a == 'c') return 1;
  return 0;
}

static mjx_box* layout_text_token(mjx_layout_ctx* ctx, mjx_node* node, int display) {
  const char* text = node->text;
  size_t len = node->text_len;
  if (!text || len == 0) return mjx_box_create(MJX_BOX_HBOX);

  mjx_box* row = mjx_box_create(MJX_BOX_HBOX);
  if (!row) return NULL;
  row->tex_class = node->tex_class;

  double fs = mjx_current_font_size(ctx, node->script_level);
  double saved = ctx->font_size;
  ctx->font_size = fs;

  for (size_t i = 0; i < len; i++) {
    uint32_t cp = (unsigned char)text[i];
    if ((cp & 0x80) == 0) {
    } else if ((cp & 0xE0) == 0xC0 && i + 1 < len) {
      cp = ((cp & 0x1F) << 6) | ((unsigned char)text[i+1] & 0x3F);
      i++;
    } else if ((cp & 0xF0) == 0xE0 && i + 2 < len) {
      cp = ((cp & 0x0F) << 12) | (((unsigned char)text[i+1] & 0x3F) << 6) | ((unsigned char)text[i+2] & 0x3F);
      i += 2;
    } else if ((cp & 0xF8) == 0xF0 && i + 3 < len) {
      cp = ((cp & 0x07) << 18) | (((unsigned char)text[i+1] & 0x3F) << 12) | (((unsigned char)text[i+2] & 0x3F) << 6) | ((unsigned char)text[i+3] & 0x3F);
      i += 3;
    }
    cp = layout_variant_codepoint(node, cp);
    if (node->type == MJX_NODE_MO) {
      mjx_box* glyph = mjx_box_create_glyph(ctx, cp);
      if (glyph) {
        glyph->tex_class = node->tex_class;
        glyph = shift_operator_glyph(glyph, delimiter_axis_shift(ctx, glyph, cp));
        glyph->tex_class = node->tex_class;
        mjx_box_add_child(row, glyph, row->width, 0);
        row->width += glyph->width;
        if (glyph->height > row->height) row->height = glyph->height;
        if (glyph->depth > row->depth) row->depth = glyph->depth;
      }
    } else {
      mjx_box* glyph = mjx_box_create_glyph(ctx, cp);
      if (glyph) {
        mjx_box_add_child(row, glyph, row->width, 0);
        row->width += glyph->width;
        if (glyph->height > row->height) row->height = glyph->height;
        if (glyph->depth > row->depth) row->depth = glyph->depth;
      }
    }
  }

  ctx->font_size = saved;
  return row;
}

static mjx_box* layout_operator(mjx_layout_ctx* ctx, mjx_node* node, int display);
static mjx_box* layout_table(mjx_layout_ctx* ctx, mjx_node* node, int display);
static mjx_box* layout_limits(mjx_layout_ctx* ctx, mjx_node* node, int display);
static mjx_box* make_stretched_arrow(mjx_layout_ctx* ctx, mjx_node* node, double target_w);

static uint32_t decode_first_codepoint(const char* text, size_t len) {
  if (!text || len == 0) return 0;
  uint32_t cp = (unsigned char)text[0];
  if ((cp & 0x80) == 0) return cp;
  if ((cp & 0xE0) == 0xC0 && len >= 2) {
    return ((cp & 0x1F) << 6) | ((unsigned char)text[1] & 0x3F);
  }
  if ((cp & 0xF0) == 0xE0 && len >= 3) {
    return ((cp & 0x0F) << 12) |
           (((unsigned char)text[1] & 0x3F) << 6) |
           ((unsigned char)text[2] & 0x3F);
  }
  if ((cp & 0xF8) == 0xF0 && len >= 4) {
    return ((cp & 0x07) << 18) |
           (((unsigned char)text[1] & 0x3F) << 12) |
           (((unsigned char)text[2] & 0x3F) << 6) |
           ((unsigned char)text[3] & 0x3F);
  }
  return cp;
}

static int is_accent_node(mjx_node* node) {
  if (!node || node->type != MJX_NODE_MO || !node->text) return 0;
  uint32_t cp = decode_first_codepoint(node->text, node->text_len);
  return ((cp >= 0x0300 && cp <= 0x036F) ||
          cp == 0x02C6 || cp == 0x02DC || cp == 0x00AF ||
          cp == 0x02D9 || cp == 0x00A8 || cp == 0x20D7 ||
          cp == 0x203E || cp == 0x2190 || cp == 0x2192 ||
          cp == 0x2194 || cp == 0x23B4 || cp == 0x23B5 ||
          cp == 0x23DC || cp == 0x23DD || cp == 0x23DE ||
          cp == 0x23DF);
}

static int is_under_accent_node(mjx_node* node) {
  if (!node || node->type != MJX_NODE_MO || !node->text) return 0;
  uint32_t cp = decode_first_codepoint(node->text, node->text_len);
  return cp == 0x203E || cp == 0x2190 || cp == 0x2192 ||
         cp == 0x2194 || cp == 0x23B5 || cp == 0x23DD || cp == 0x23DF;
}

static double delimiter_axis_shift(mjx_layout_ctx* ctx, mjx_box* glyph, uint32_t cp) {
  if (!ctx || !glyph) return 0.0;
  if (cp != '(' && cp != ')' && cp != '[' && cp != ']') return 0.0;

  double scale = (ctx->font && ctx->font->em_size > 0.0) ?
    ctx->font_size / ctx->font->em_size : 1.0;
  double axis = ctx->mc ? ctx->mc->axis_height * scale : ctx->font_size * 0.25;
  double center = (glyph->depth - glyph->height) / 2.0;
  double target = -axis - ctx->font_size * 0.045;
  double shift = target - center;
  return shift > 0.0 ? shift : 0.0;
}

static mjx_box* shift_operator_glyph(mjx_box* glyph, double shift) {
  if (!glyph || shift <= 0.0) return glyph;

  mjx_box* wrapper = mjx_box_create(MJX_BOX_HBOX);
  if (!wrapper) return glyph;
  wrapper->tex_class = glyph->tex_class;
  mjx_box_add_child(wrapper, glyph, 0, -shift);
  mjx_box_hpack(wrapper);
  return wrapper;
}

static mjx_box* make_stretched_arrow_codepoint(mjx_layout_ctx* ctx, uint32_t cp, double target_w) {
  if (!ctx) return NULL;
  double min_w = ctx->font_size * 1.2;
  if (target_w < min_w) target_w = min_w;

  if (cp == 0x21CB || cp == 0x21CC) {
    uint32_t top_cp = (cp == 0x21CC) ? 0x21C0 : 0x21BC;
    uint32_t bottom_cp = (cp == 0x21CC) ? 0x21BD : 0x21C1;
    mjx_box* top = make_stretched_arrow_codepoint(ctx, top_cp, target_w);
    mjx_box* bottom = make_stretched_arrow_codepoint(ctx, bottom_cp, target_w);
    if (!top || !bottom) {
      if (top) mjx_box_destroy(top);
      if (bottom) mjx_box_destroy(bottom);
      return NULL;
    }

    mjx_box* row = mjx_box_create(MJX_BOX_HBOX);
    if (!row) {
      mjx_box_destroy(top);
      mjx_box_destroy(bottom);
      return NULL;
    }
    row->tex_class = MJX_TEXCLASS_REL;

    double gap = ctx->font_size * 0.12;
    double total_w = fmax(top->width, bottom->width);
    mjx_box_add_child(row, top, (total_w - top->width) / 2.0, -gap);
    mjx_box_add_child(row, bottom, (total_w - bottom->width) / 2.0, gap);
    mjx_box_hpack(row);
    row->tex_class = MJX_TEXCLASS_REL;
    return row;
  }

  int left_head = 0, right_head = 0;
  int double_rule = 0;
  int two_head = 0;
  int mapsto = 0;
  switch (cp) {
    case 0x2190: left_head = 1; break;
    case 0x2192: right_head = 1; break;
    case 0x2194: left_head = 1; right_head = 1; break;
    case 0x21D0: left_head = 1; double_rule = 1; break;
    case 0x21D2: right_head = 1; double_rule = 1; break;
    case 0x21D4: left_head = 1; right_head = 1; double_rule = 1; break;
    case 0x21A6: right_head = 1; mapsto = 1; break;
    case 0x219E: left_head = 1; two_head = 1; break;
    case 0x21A0: right_head = 1; two_head = 1; break;
    case 0x21A9:
    case 0x21AA:
    case 0x21BC:
    case 0x21BD:
    case 0x21C0:
    case 0x21C1:
      break;
    default: return NULL;
  }

  {
    double scale = (ctx->font && ctx->font->em_size > 0) ?
      ctx->font_size / ctx->font->em_size : 1.0;
    mjx_glyph_variant variants[16];
    unsigned int nvariants = 0;
    if (mjx_font_get_horizontal_variants(ctx->font, cp, variants, 16, &nvariants) && nvariants > 0) {
      unsigned int best = 0;
      for (unsigned int i = 0; i < nvariants && i < 16; i++) {
        best = i;
        if (variants[i].advance_width * scale >= target_w) break;
      }
      if (variants[best].advance_width * scale >= target_w * 0.9) {
        mjx_box* glyph = mjx_box_create(MJX_BOX_GLYPH);
        if (glyph) {
          glyph->codepoint = cp;
          glyph->glyph_id = variants[best].glyph_id;
          glyph->font_size = ctx->font_size;
          glyph->width = variants[best].advance_width * scale;
          glyph->height = ctx->font_size * 0.42;
          glyph->depth = ctx->font_size * 0.12;
          glyph->tex_class = MJX_TEXCLASS_REL;
          return glyph;
        }
      }
    }

    mjx_glyph_part parts[32];
    unsigned int np = 0;
    double ic = 0.0;
    int have_parts = mjx_font_get_horizontal_parts(ctx->font, cp, parts, 32, &np, &ic);
    if (!have_parts || np == 0) {
      const mjx_stix_glyph_part* stix_parts = NULL;
      unsigned int stix_count = 0;
      if (mjx_stix_get_horizontal_parts(cp, &stix_parts, &stix_count) &&
          stix_parts && stix_count > 0) {
        np = stix_count < 32 ? stix_count : 32;
        for (unsigned int i = 0; i < np; i++) {
          parts[i].glyph_id = stix_parts[i].glyph_id;
          parts[i].is_extender = stix_parts[i].is_extender;
          parts[i].full_advance = stix_parts[i].full_advance_units *
                                  ctx->font_size / 1000.0;
        }
        have_parts = 1;
      }
    }
    if (have_parts && np > 0) {
      double total_adv = 0.0;
      double extender_adv = 0.0;
      int has_extender = 0;
      for (unsigned int i = 0; i < np && i < 32; i++) {
        double adv = parts[i].full_advance * scale;
        total_adv += adv;
        if (parts[i].is_extender) {
          extender_adv = adv;
          has_extender = 1;
        }
      }

      int repeat_count = 1;
      if (has_extender && extender_adv > 0.0 && target_w > total_adv) {
        double base = total_adv - extender_adv;
        repeat_count = (int)ceil((target_w - base) / extender_adv);
        if (repeat_count < 1) repeat_count = 1;
      }

      mjx_box* row = mjx_box_create(MJX_BOX_HBOX);
      if (row) {
        row->tex_class = MJX_TEXCLASS_REL;
        double x = 0.0;
        double max_h = 0.0;
        for (unsigned int i = 0; i < np && i < 32; i++) {
          int repeat = parts[i].is_extender ? repeat_count : 1;
          for (int r = 0; r < repeat; r++) {
            mjx_box* part = mjx_box_create(MJX_BOX_GLYPH);
            if (!part) continue;
            part->glyph_id = parts[i].glyph_id;
            part->font_size = ctx->font_size;
            part->width = mjx_font_glyph_width(ctx->font, parts[i].glyph_id) * scale;
            part->height = mjx_font_glyph_height(ctx->font, parts[i].glyph_id) * scale;
            part->depth = 0.0;
            if (part->height > max_h) max_h = part->height;
            mjx_box_add_child(row, part, x, 0.0);
            {
              double adv = parts[i].full_advance * scale;
              x += adv > 0.0 ? adv : part->width;
            }
          }
        }
        row->width = x > 0.0 ? x : target_w;
        row->height = fmax(max_h * 0.65, ctx->font_size * 0.42);
        row->depth = fmax(max_h * 0.35, ctx->font_size * 0.12);
        return row;
      }
    }
  }

  {
    uint32_t long_cp = 0;
    switch (cp) {
      case 0x2190: long_cp = 0x27F5; break;
      case 0x2192: long_cp = 0x27F6; break;
      case 0x2194: long_cp = 0x27F7; break;
      case 0x21D0: long_cp = 0x27F8; break;
      case 0x21D2: long_cp = 0x27F9; break;
      case 0x21D4: long_cp = 0x27FA; break;
      default: break;
    }
    if (long_cp && mjx_font_has_glyph(ctx->font, long_cp)) {
      mjx_box* glyph = mjx_box_create_glyph(ctx, long_cp);
      if (glyph) {
        glyph->tex_class = MJX_TEXCLASS_REL;
        if (glyph->width >= target_w * 0.82) {
          return glyph;
        }

        mjx_box* row = mjx_box_create(MJX_BOX_HBOX);
        if (!row) return glyph;
        row->tex_class = MJX_TEXCLASS_REL;

        double thick = ctx->font_size * (double_rule ? 0.035 : 0.025);
        if (thick < 1.0) thick = 1.0;
        double overlap = ctx->font_size * 0.18;
        double rule_w = target_w - glyph->width + overlap;
        if (rule_w < ctx->font_size * 0.4) rule_w = ctx->font_size * 0.4;
        double rule_y = -thick / 2.0;

        if (cp == 0x2190 || cp == 0x21D0 || cp == 0x2194 || cp == 0x21D4) {
          mjx_box_add_child(row, glyph, 0, 0);
          mjx_box* rule = mjx_box_create_rule(rule_w, thick, 0);
          if (rule) mjx_box_add_child(row, rule, glyph->width - overlap, rule_y);
          if (double_rule) {
            mjx_box* rule2 = mjx_box_create_rule(rule_w, thick, 0);
            if (rule2) mjx_box_add_child(row, rule2, glyph->width - overlap, rule_y + ctx->font_size * 0.12);
          }
        } else {
          mjx_box* rule = mjx_box_create_rule(rule_w, thick, 0);
          if (rule) mjx_box_add_child(row, rule, 0, rule_y);
          if (double_rule) {
            mjx_box* rule2 = mjx_box_create_rule(rule_w, thick, 0);
            if (rule2) mjx_box_add_child(row, rule2, 0, rule_y + ctx->font_size * 0.12);
          }
          mjx_box_add_child(row, glyph, rule_w - overlap, 0);
        }

        mjx_box_hpack(row);
        row->height = fmax(row->height, ctx->font_size * 0.42);
        row->depth = fmax(row->depth, ctx->font_size * 0.12);
        return row;
      }
    }
  }

  if (cp == 0x21A9 || cp == 0x21AA) {
    uint32_t fallback_cp = cp;
    mjx_box* glyph = mjx_box_create_glyph(ctx, fallback_cp);
    if (!glyph) return NULL;
    mjx_box* row = mjx_box_create(MJX_BOX_HBOX);
    if (!row) return glyph;
    row->tex_class = MJX_TEXCLASS_REL;
    double rule_w = fmax(ctx->font_size * 0.7, target_w - glyph->width + ctx->font_size * 0.08);
    double thick = fmax(1.0, ctx->font_size * 0.025);
    mjx_box* rule = mjx_box_create_rule(rule_w, thick, 0);
    if (cp == 0x21A9) {
      mjx_box_add_child(row, glyph, 0, 0);
      if (rule) mjx_box_add_child(row, rule, glyph->width - ctx->font_size * 0.12, -thick / 2.0);
    } else {
      if (rule) mjx_box_add_child(row, rule, 0, -thick / 2.0);
      mjx_box_add_child(row, glyph, rule_w - ctx->font_size * 0.12, 0);
    }
    mjx_box_hpack(row);
    row->height = fmax(row->height, ctx->font_size * 0.42);
    row->depth = fmax(row->depth, ctx->font_size * 0.12);
    return row;
  }

  mjx_box* row = mjx_box_create(MJX_BOX_HBOX);
  if (!row) return NULL;
  row->tex_class = MJX_TEXCLASS_REL;

  double thick = ctx->font_size * (double_rule ? 0.035 : 0.025);
  if (thick < 1.0) thick = 1.0;
  double head_len = ctx->font_size * 0.28;
  double head_rise = ctx->font_size * 0.16;
  double head_gap = two_head ? ctx->font_size * 0.16 : 0.0;
  double start_x = left_head ? head_len + head_gap : 0.0;
  if (mapsto) start_x += ctx->font_size * 0.12;
  double end_x = target_w - (right_head ? (head_len + head_gap) : 0.0);
  if (end_x < start_x + ctx->font_size * 0.5) end_x = start_x + ctx->font_size * 0.5;
  double rule_w = end_x - start_x;
  double y = -thick / 2.0;

  if (mapsto) {
    mjx_box* bar = mjx_box_create(MJX_BOX_DIAGONAL);
    if (bar) {
      bar->width = thick;
      bar->height = ctx->font_size * 0.42;
      bar->diag_x1 = thick / 2.0;
      bar->diag_y1 = 0;
      bar->diag_x2 = thick / 2.0;
      bar->diag_y2 = bar->height;
      bar->diag_thickness = thick;
      mjx_box_add_child(row, bar, 0, ctx->font_size * 0.21);
    }
  }

  mjx_box* rule = mjx_box_create_rule(rule_w, thick, 0);
  if (rule) mjx_box_add_child(row, rule, start_x, y);
  if (double_rule) {
    mjx_box* rule2 = mjx_box_create_rule(rule_w, thick, 0);
    if (rule2) mjx_box_add_child(row, rule2, start_x, y + ctx->font_size * 0.12);
  }

  for (int side = 0; side < 2; side++) {
    int is_left = side == 0;
    int heads = is_left ? left_head : right_head;
    if (!heads) continue;
    double tip_x = is_left ? 0.0 : target_w;
    for (int h = 0; h < (two_head ? 2 : 1); h++) {
      double tip = tip_x + (is_left ? h * head_gap : -h * head_gap);
      double base = tip + (is_left ? head_len : -head_len);
      mjx_box* upper = mjx_box_create(MJX_BOX_DIAGONAL);
      mjx_box* lower = mjx_box_create(MJX_BOX_DIAGONAL);
      if (upper) {
        upper->width = head_len;
        upper->height = head_rise;
        upper->diag_x1 = is_left ? head_len : 0;
        upper->diag_y1 = 0;
        upper->diag_x2 = is_left ? 0 : head_len;
        upper->diag_y2 = head_rise;
        upper->diag_thickness = thick;
        mjx_box_add_child(row, upper, fmin(tip, base), 0);
      }
      if (lower) {
        lower->width = head_len;
        lower->height = head_rise;
        lower->diag_x1 = is_left ? head_len : 0;
        lower->diag_y1 = head_rise;
        lower->diag_x2 = is_left ? 0 : head_len;
        lower->diag_y2 = 0;
        lower->diag_thickness = thick;
        mjx_box_add_child(row, lower, fmin(tip, base), head_rise);
      }
    }
  }

  row->width = target_w;
  row->height = fmax(ctx->font_size * 0.42, head_rise);
  row->depth = fmax(ctx->font_size * 0.12, head_rise);
  return row;
}

static mjx_box* make_stretched_arrow(mjx_layout_ctx* ctx, mjx_node* node, double target_w) {
  if (!ctx || !node || !node->text) return NULL;
  return make_stretched_arrow_codepoint(ctx,
                                        decode_first_codepoint(node->text, node->text_len),
                                        target_w);
}

static mjx_box* make_sized_accent_glyph(mjx_layout_ctx* ctx, const unsigned int* glyphs,
                                         unsigned int count, mjx_box* base,
                                         double min_width_em, double stretch_tolerance) {
  if (!ctx || !ctx->font || !glyphs || count == 0) return NULL;

  double metric_scale = (ctx->font->em_size > 0.0) ? ctx->font_size / ctx->font->em_size : 1.0;
  double target_w = ctx->font_size * min_width_em;
  if (base && base->width > 0.0) target_w = fmax(target_w, base->width * 0.94);

  unsigned int best_gid = 0;
  double best_w = 0.0;
  double best_h = 0.0;
  double best_depth = 0.0;

  for (unsigned int i = 0; i < count; i++) {
    double w = mjx_font_glyph_width(ctx->font, glyphs[i]) * metric_scale;
    double h = mjx_font_glyph_height(ctx->font, glyphs[i]) * metric_scale;
    if (w <= 0.0 || h <= 0.0) continue;
    best_gid = glyphs[i];
    best_w = w;
    best_h = h;
    {
      mjx_glyph_id_info info;
      if (mjx_font_get_glyph_id_info(ctx->font, glyphs[i], &info)) {
        best_depth = info.depth * metric_scale;
      } else {
        best_depth = 0.0;
      }
    }
    if (target_w <= w * stretch_tolerance) break;
  }
  if (best_gid == 0) return NULL;

  mjx_box* accent = mjx_box_create(MJX_BOX_GLYPH);
  if (!accent) return NULL;
  accent->glyph_id = best_gid;
  accent->font_size = ctx->font_size;
  accent->width = best_w;
  accent->height = best_h;
  accent->depth = 0.0;
  accent->bb_y = -best_depth;
  accent->tex_class = MJX_TEXCLASS_ORD;

  if (target_w > best_w * stretch_tolerance) {
    accent->width = target_w;
    accent->allow_nonuniform_scale = 1;
  }
  return accent;
}

static void align_decorator_as_under(mjx_layout_ctx* ctx, mjx_box* box) {
  if (!ctx || !box || box->glyph_id == 0) return;

  double metric_scale = (ctx->font && ctx->font->em_size > 0.0) ?
    box->font_size / ctx->font->em_size : 1.0;
  mjx_glyph_id_info info;
  if (!mjx_font_get_glyph_id_info(ctx->font, box->glyph_id, &info)) return;

  double visual_h = (info.height + info.depth) * metric_scale;
  double y_max = info.height * metric_scale;
  if (visual_h <= 0.0) return;

  box->bb_y = y_max;
  box->height = 0.0;
  box->depth = visual_h;
}

static mjx_box* make_font_first_accent(mjx_layout_ctx* ctx, uint32_t accent_cp,
                                       mjx_box* base) {
  mjx_stix_accent_variants variants;
  if (!mjx_stix_get_accent_variants(accent_cp, &variants)) return NULL;
  return make_sized_accent_glyph(ctx, variants.glyphs, variants.count, base,
                                 variants.min_width_factor,
                                 variants.stretch_tolerance);
}

static mjx_box* make_natural_accent(mjx_layout_ctx* ctx, uint32_t accent_cp) {
  mjx_stix_accent_variants variants;
  if (!mjx_stix_get_accent_variants(accent_cp, &variants) ||
      !variants.glyphs || variants.count == 0) {
    return NULL;
  }
  return make_sized_accent_glyph(ctx, variants.glyphs, 1, NULL, 0.0, 1.0);
}

static double base_top_accent_x(mjx_layout_ctx* ctx, mjx_box* base) {
  if (!ctx || !ctx->font || !base || base->codepoint == 0) {
    return base ? base->width / 2.0 : 0.0;
  }

  mjx_glyph_info info;
  memset(&info, 0, sizeof(info));
  if (!mjx_font_get_glyph(ctx->font, base->codepoint, &info) ||
      info.top_accent_attachment <= 0.0) {
    return base->width / 2.0;
  }

  double scale = (ctx->font->em_size > 0.0) ? base->font_size / ctx->font->em_size : 1.0;
  return info.top_accent_attachment * scale;
}

static mjx_box* layout_node_inner(mjx_layout_ctx* ctx, mjx_node* node, int display) {
  if (!node) return NULL;

  switch (node->type) {
    case MJX_NODE_MROW:
    case MJX_NODE_MPHANTOM: {
      const char* left_delim = mjx_node_get_attr(node, "left");
      const char* right_delim = mjx_node_get_attr(node, "right");
      if ((left_delim && left_delim[0]) || (right_delim && right_delim[0])) {
        const char* fixed = mjx_node_get_attr(node, "fixed_delims");
        mjx_box* inner = mjx_layout_row(ctx, node->children, node->child_count, display);
        return mjx_layout_delimited(ctx, inner, left_delim, right_delim,
                                    fixed && strcmp(fixed, "true") == 0);
      }
      return mjx_layout_row(ctx, node->children, node->child_count, display);
    }

    case MJX_NODE_MI:
    case MJX_NODE_MN:
    case MJX_NODE_MTEXT:
      return layout_text_token(ctx, node, display);

    case MJX_NODE_MSPACE:
      if (node->text && node->text[0]) {
        return layout_text_token(ctx, node, display);
      }
      {
        mjx_box* space = mjx_box_create(MJX_BOX_GLUE);
        if (!space) return mjx_box_create(MJX_BOX_HBOX);
        double w = 0;
        const char* wa = mjx_node_get_attr(node, "width");
        if (wa) {
          if (strstr(wa, "mu")) w = atof(wa) * ctx->font_size / 18.0;
          else if (strstr(wa, "em")) w = atof(wa) * ctx->font_size;
          else if (strstr(wa, "pt")) w = atof(wa) * ctx->font_size / 72.0;
          else w = atof(wa) * ctx->font_size;
        }
        space->width = w;
        {
          const char* ha = mjx_node_get_attr(node, "height");
          const char* da = mjx_node_get_attr(node, "depth");
          if (ha) {
            if (strstr(ha, "mu")) space->height = atof(ha) * ctx->font_size / 18.0;
            else if (strstr(ha, "em")) space->height = atof(ha) * ctx->font_size;
            else if (strstr(ha, "pt")) space->height = atof(ha) * ctx->font_size / 72.0;
            else space->height = atof(ha) * ctx->font_size;
          }
          if (da) {
            if (strstr(da, "mu")) space->depth = atof(da) * ctx->font_size / 18.0;
            else if (strstr(da, "em")) space->depth = atof(da) * ctx->font_size;
            else if (strstr(da, "pt")) space->depth = atof(da) * ctx->font_size / 72.0;
            else space->depth = atof(da) * ctx->font_size;
          }
        }
        return space;
      }

    case MJX_NODE_MO:
      return layout_operator(ctx, node, display);

    case MJX_NODE_MSUP:
    case MJX_NODE_MSUB:
    case MJX_NODE_MSUBSUP:
      return mjx_layout_scripts(ctx, node, display);

    case MJX_NODE_MFRAC:
      return mjx_layout_fraction(ctx, node, display);

    case MJX_NODE_MSQRT:
      return mjx_layout_sqrt(ctx, node, display);

    case MJX_NODE_MROOT:
      return mjx_layout_root(ctx, node, display);

    case MJX_NODE_MUNDER:
    case MJX_NODE_MOVER:
    case MJX_NODE_MUNDEROVER:
      return layout_limits(ctx, node, display);

    case MJX_NODE_MTABLE:
      return layout_table(ctx, node, display);

    case MJX_NODE_MSTYLE:
      if (node->child_count > 0) {
        return mjx_layout_row(ctx, node->children, node->child_count, display);
      }
      return mjx_box_create(MJX_BOX_HBOX);

    case MJX_NODE_TEXATOM: {
      mjx_box* inner = mjx_layout_row(ctx, node->children, node->child_count, display);
      if (inner) {
        return mjx_box_create_atom(inner, node->tex_class);
      }
      return mjx_box_create(MJX_BOX_HBOX);
    }

    case MJX_NODE_MERROR:
      if (node->child_count > 0) {
        return mjx_layout_row(ctx, node->children, node->child_count, display);
      }
      return mjx_box_create(MJX_BOX_HBOX);

    case MJX_NODE_MSEMANTICS:
      if (node->child_count > 0) {
        return mjx_layout_node(ctx, node->children[0], display);
      }
      return mjx_box_create(MJX_BOX_HBOX);

    case MJX_NODE_TEXT:
      return layout_text_token(ctx, node, display);

    case MJX_NODE_MMULTISCRIPTS:
      return mjx_layout_scripts(ctx, node, display);

    case MJX_NODE_MPADDED: {
      mjx_box* inner = mjx_layout_row(ctx, node->children, node->child_count, display);
      if (!inner) return mjx_box_create(MJX_BOX_HBOX);
      const char* w = mjx_node_get_attr(node, "width");
      const char* vo = mjx_node_get_attr(node, "voffset");
      if (w) {
        if (strstr(w, "em")) inner->width = atof(w) * ctx->font_size;
        else inner->width = atof(w);
      }
      if (vo) {
        double v = 0;
        if (strstr(vo, "em")) v = atof(vo) * ctx->font_size;
        else v = atof(vo);
        inner->height += v;
      }
      return inner;
    }

    case MJX_NODE_MGLYPH: {
      mjx_box* g = mjx_box_create(MJX_BOX_GLYPH);
      if (g) {
        g->font_size = ctx->font_size;
        const char* src = mjx_node_get_attr(node, "src");
        const char* alt = mjx_node_get_attr(node, "alt");
        if (alt && alt[0]) {
          g->width = atof(alt) * 0.5 * ctx->font_size;
          g->height = ctx->font_size * 0.7;
          g->depth = ctx->font_size * 0.1;
        } else {
          g->width = ctx->font_size * 0.5;
          g->height = ctx->font_size * 0.7;
          g->depth = ctx->font_size * 0.1;
        }
        (void)src;
      }
      return g;
    }

    case MJX_NODE_MENCLOSE:
      if (node->child_count > 0) {
        mjx_box* inner = mjx_layout_node(ctx, node->children[0], display);
        if (inner) {
          const char* notation = mjx_node_get_attr(node, "notation");
          int is_cancel = notation &&
            (strcmp(notation, "cancel") == 0 ||
             strcmp(notation, "bcancel") == 0 ||
             strcmp(notation, "xcancel") == 0);

          if (is_cancel) {
            double thick = ctx->font_size * 0.04;
            double w = inner->width;
            double h = inner->height + inner->depth;
            mjx_box* boxed = mjx_box_create(MJX_BOX_VBOX);
            mjx_box_add_child(boxed, inner, 0, -(inner->height));
            boxed->width = inner->width;
            boxed->height = inner->height;
            boxed->depth = inner->depth;

            if (strcmp(notation, "cancel") == 0 || strcmp(notation, "xcancel") == 0) {
              mjx_box* diag = mjx_box_create(MJX_BOX_DIAGONAL);
              diag->diag_x1 = 0; diag->diag_y1 = h;
              diag->diag_x2 = w; diag->diag_y2 = 0;
              diag->diag_thickness = thick;
              diag->width = w; diag->height = h;
              mjx_box_add_child(boxed, diag, 0, 0);
            }
            if (strcmp(notation, "bcancel") == 0 || strcmp(notation, "xcancel") == 0) {
              mjx_box* diag = mjx_box_create(MJX_BOX_DIAGONAL);
              diag->diag_x1 = 0; diag->diag_y1 = 0;
              diag->diag_x2 = w; diag->diag_y2 = h;
              diag->diag_thickness = thick;
              diag->width = w; diag->height = h;
              mjx_box_add_child(boxed, diag, 0, 0);
            }
            return boxed;
          }

          mjx_box* boxed = mjx_box_create(MJX_BOX_HBOX);
          if (boxed) {
            double pad_x = ctx->font_size * 0.16;
            double pad_y = ctx->font_size * 0.12;
            double thick = ctx->font_size * 0.035;
            if (thick < 1.0) thick = 1.0;

            double width = inner->width + 2.0 * pad_x + 2.0 * thick;
            double height = inner->height + pad_y + thick;
            double depth = inner->depth + pad_y + thick;
            double top = -height;
            double bottom = depth;
            double total_h = height + depth;

            mjx_box_add_child(boxed, inner, pad_x + thick, 0);

            mjx_box* top_rule = mjx_box_create_rule(width, thick, 0);
            mjx_box* bottom_rule = mjx_box_create_rule(width, thick, 0);
            if (top_rule) mjx_box_add_child(boxed, top_rule, 0, top);
            if (bottom_rule) mjx_box_add_child(boxed, bottom_rule, 0, bottom - thick);

            mjx_box* left_rule = mjx_box_create(MJX_BOX_DIAGONAL);
            if (left_rule) {
              left_rule->width = thick;
              left_rule->height = total_h;
              left_rule->diag_x1 = thick / 2.0;
              left_rule->diag_y1 = 0;
              left_rule->diag_x2 = thick / 2.0;
              left_rule->diag_y2 = total_h;
              left_rule->diag_thickness = thick;
              mjx_box_add_child(boxed, left_rule, thick / 2.0, bottom);
            }

            mjx_box* right_rule = mjx_box_create(MJX_BOX_DIAGONAL);
            if (right_rule) {
              right_rule->width = thick;
              right_rule->height = total_h;
              right_rule->diag_x1 = thick / 2.0;
              right_rule->diag_y1 = 0;
              right_rule->diag_x2 = thick / 2.0;
              right_rule->diag_y2 = total_h;
              right_rule->diag_thickness = thick;
              mjx_box_add_child(boxed, right_rule, width - thick * 1.5, bottom);
            }

            boxed->width = width;
            boxed->height = height;
            boxed->depth = depth;
            boxed->tex_class = MJX_TEXCLASS_ORD;
            return boxed;
          }
          return inner;
        }
      }
      return mjx_box_create(MJX_BOX_HBOX);

    case MJX_NODE_MACTION:
      if (node->child_count > 0) {
        return mjx_layout_node(ctx, node->children[0], display);
      }
      return mjx_box_create(MJX_BOX_HBOX);

    default:
      return mjx_layout_row(ctx, node->children, node->child_count, display);
  }
}

mjx_box* mjx_layout_node(mjx_layout_ctx* ctx, mjx_node* node, int display) {
  if (!ctx || !node) return NULL;
  double saved_size = ctx->font_size;
  double saved_mu = ctx->mu_unit;
  ctx->font_size = mjx_current_font_size(ctx, node->script_level);
  ctx->mu_unit = ctx->font_size / 18.0;
  mjx_box* box = layout_node_inner(ctx, node, node->display_style);
  ctx->font_size = saved_size;
  ctx->mu_unit = saved_mu;
  (void)display;
  return box;
}

mjx_box* mjx_layout_row(mjx_layout_ctx* ctx, mjx_node** children, size_t count, int display) {
  mjx_box* row = mjx_box_create(MJX_BOX_HBOX);
  if (!row) return NULL;

  if (count == 0) return row;

  mjx_texclass prev_class = MJX_TEXCLASS_NONE;

  for (size_t i = 0; i < count; i++) {
    if (!children[i]) continue;

    int script_level = children[i]->script_level;
    double saved_size = ctx->font_size;
    ctx->font_size = mjx_current_font_size(ctx, script_level);

    mjx_box* child_box = mjx_layout_node(ctx, children[i], display);
    if (!child_box) { ctx->font_size = saved_size; continue; }

    mjx_texclass cur_class = child_box->tex_class;
    if (cur_class == MJX_TEXCLASS_NONE) {
      cur_class = MJX_TEXCLASS_ORD;
    }
    if (cur_class == MJX_TEXCLASS_BIN &&
        (prev_class == MJX_TEXCLASS_NONE ||
         prev_class == MJX_TEXCLASS_BIN ||
         prev_class == MJX_TEXCLASS_REL ||
         prev_class == MJX_TEXCLASS_OPEN ||
         prev_class == MJX_TEXCLASS_PUNCT)) {
      cur_class = MJX_TEXCLASS_ORD;
    }

    int spacing_level = script_level;
    if (spacing_level == 0 && ctx->script_depth > 0) spacing_level = 1;
    double space = mjx_tex_spacing(prev_class, cur_class, spacing_level);
    if (space >= 0) {
      row->width += space * ctx->font_size;
    }

    mjx_box_add_child(row, child_box, row->width, 0);
    row->width += child_box->width;

    if (child_box->height > row->height) row->height = child_box->height;
    if (child_box->depth > row->depth) row->depth = child_box->depth;

    prev_class = cur_class;
    ctx->font_size = saved_size;
  }

  return row;
}

static mjx_box* layout_operator(mjx_layout_ctx* ctx, mjx_node* node, int display) {
  if (!node->text || !node->text[0]) return mjx_box_create(MJX_BOX_HBOX);

  double fs = mjx_current_font_size(ctx, node->script_level);
  double saved = ctx->font_size;
  ctx->font_size = fs;

  {
    const char* glyph_id_attr = mjx_node_get_attr(node, "glyph_id");
    if (glyph_id_attr && glyph_id_attr[0]) {
      unsigned int glyph_id = (unsigned int)strtoul(glyph_id_attr, NULL, 10);
      mjx_glyph_id_info info;
      if (glyph_id != 0 && mjx_font_get_glyph_id_info(ctx->font, glyph_id, &info)) {
        mjx_box* box = mjx_box_create(MJX_BOX_GLYPH);
        if (box) {
          box->glyph_id = glyph_id;
          box->font_size = ctx->font_size;
          box->width = info.advance_width;
          box->height = info.height;
          box->depth = info.depth;
          box->tex_class = node->tex_class;
          ctx->font_size = saved;
          return box;
        }
      }
    }
  }

  /* For large ops in display style, try to use a larger glyph variant */
  if (node->mo_largeop && display && node->script_level == 0 && node->text_len > 0) {
    uint32_t cp = (unsigned char)node->text[0];
    if (cp < 0x80 && node->text_len > 1) {
      /* Multi-byte: decode UTF-8 */
      if ((cp & 0xE0) == 0xC0 && node->text_len >= 2) {
        cp = ((cp & 0x1F) << 6) | ((unsigned char)node->text[1] & 0x3F);
      } else if ((cp & 0xF0) == 0xE0 && node->text_len >= 3) {
        cp = ((cp & 0x0F) << 12) | (((unsigned char)node->text[1] & 0x3F) << 6) | ((unsigned char)node->text[2] & 0x3F);
      }
    }

    mjx_glyph_variant variants[16];
    unsigned int var_count = 0;
    if (mjx_font_get_variants(ctx->font, cp, variants, 16, &var_count) && var_count > 0) {
      /* Use last (largest) variant for display style */
      unsigned int best = var_count - 1;
      double scale = (ctx->font && ctx->font->em_size > 0) ?
        ctx->font_size / ctx->font->em_size : 1.0;
      mjx_box* box = mjx_box_create(MJX_BOX_GLYPH);
      if (box) {
        box->codepoint = cp;
        box->glyph_id = variants[best].glyph_id;
        box->font_size = ctx->font_size;
        box->width = variants[best].advance_width * scale;
        box->height = variants[best].height * scale;
        box->tex_class = node->tex_class;
        ctx->font_size = saved;
        return box;
      }
    }
  }

  mjx_box* result = layout_text_token(ctx, node, display);
  if (result) result->tex_class = node->tex_class;

  ctx->font_size = saved;
  return result;
}

static mjx_box* layout_limits(mjx_layout_ctx* ctx, mjx_node* node, int display) {
  if (node->child_count == 0) return mjx_box_create(MJX_BOX_HBOX);

  mjx_box* base = mjx_layout_node(ctx, node->children[0], display);
  if (!base) return mjx_box_create(MJX_BOX_HBOX);
  if (node->child_count == 1) return base;

  mjx_math_constants* mc = ctx->mc;
  double saved = ctx->font_size;
  int accent_mode = (node->type == MJX_NODE_MOVER && is_accent_node(node->children[1]));
  int under_accent_mode = (node->type == MJX_NODE_MUNDER && is_under_accent_node(node->children[1]));
  const char* accent_kind = (accent_mode || under_accent_mode) ?
    mjx_node_get_attr(node->children[1], "accent_kind") : NULL;
  int normal_accent = accent_kind && strcmp(accent_kind, "normal") == 0;
  uint32_t current_accent_cp = (accent_mode || under_accent_mode) ?
    decode_first_codepoint(node->children[1]->text, node->children[1]->text_len) : 0;

  ctx->font_size = saved * mjx_font_script_scale(ctx->font, 1) * 0.7;
  ctx->script_depth++;

  mjx_box* over = NULL;
  mjx_box* under = NULL;

  if (node->type == MJX_NODE_MOVER) {
    if (accent_mode) {
      uint32_t accent_cp = current_accent_cp;
      if (normal_accent) {
        ctx->font_size = (accent_cp == 0x20D7) ? saved * 0.42 : saved;
        over = (accent_cp == 0x20D7) ? make_font_first_accent(ctx, accent_cp, NULL)
                                     : make_natural_accent(ctx, accent_cp);
      } else if (accent_cp == 0x0302 || accent_cp == 0x02C6 ||
                 accent_cp == 0x030C || accent_cp == 0x02C7 ||
                 accent_cp == 0x0303 || accent_cp == 0x02DC ||
                 accent_cp == 0x0304 || accent_cp == 0x00AF ||
                 accent_cp == 0x203E) {
        ctx->font_size = saved;
        over = make_font_first_accent(ctx, accent_cp, base);
      } else if (accent_cp == 0x20D7 || accent_cp == 0x2192 ||
                 accent_cp == 0x2190 || accent_cp == 0x2194) {
        ctx->font_size = normal_accent ? saved : saved * 0.58;
        over = normal_accent ? make_natural_accent(ctx, accent_cp)
                             : make_stretched_arrow_codepoint(ctx, accent_cp,
                                                             base->width + saved * 0.18);
        if (!over) over = make_font_first_accent(ctx, accent_cp, base);
      } else if (accent_cp == 0x23B4 || accent_cp == 0x23DC ||
                 accent_cp == 0x23DE) {
        ctx->font_size = saved;
        over = make_font_first_accent(ctx, accent_cp, base);
      }
      if (!over) ctx->font_size = saved * 0.7;
    }
    if (!over) over = mjx_layout_node(ctx, node->children[1], 0);
    if (accent_mode && over) over->depth = 0;
  } else if (node->type == MJX_NODE_MUNDER) {
    if (under_accent_mode) {
      uint32_t accent_cp = current_accent_cp;
      if (accent_cp == 0x2190 || accent_cp == 0x2192 ||
          accent_cp == 0x2194) {
        ctx->font_size = saved * 0.58;
        under = make_stretched_arrow_codepoint(ctx, accent_cp, base->width + saved * 0.18);
        if (!under) under = make_font_first_accent(ctx, accent_cp, base);
      } else {
        ctx->font_size = saved;
        under = make_font_first_accent(ctx, accent_cp, base);
        align_decorator_as_under(ctx, under);
      }
      if (!under) ctx->font_size = saved * 0.7;
    }
    if (!under) under = mjx_layout_node(ctx, node->children[1], 0);
    if (under_accent_mode && under) under->depth = 0;
  } else if (node->type == MJX_NODE_MUNDEROVER) {
    under = mjx_layout_node(ctx, node->children[1], 0);
    if (node->child_count > 2) {
      over = mjx_layout_node(ctx, node->children[2], 0);
    }
  }

  ctx->script_depth--;
  ctx->font_size = saved;

  if (!over && !under) return base;

  mjx_box* result = mjx_box_create(MJX_BOX_VBOX);
  if (!result) {
    if (over) mjx_box_destroy(over);
    if (under) mjx_box_destroy(under);
    return base;
  }
  result->tex_class = base->tex_class;

  double upper_gap = (display && mc) ? mc->upper_limit_gap_min : (saved * 0.2);
  double lower_gap = (display && mc) ? mc->lower_limit_gap_min : (saved * 0.2);
  if (accent_mode) {
    upper_gap = saved * 0.035;
  }
  if (under_accent_mode) {
    lower_gap = saved * 0.035;
  }
  if (!accent_mode && !under_accent_mode && node->children[0] &&
      (node->children[0]->type == MJX_NODE_MOVER ||
       node->children[0]->type == MJX_NODE_MUNDER) &&
      !mjx_node_get_attr(node->children[0], "movablelimits")) {
    upper_gap = saved * 0.22;
    lower_gap = saved * 0.22;
  }
  double total_w = base->width;

  if (over && over->width > total_w) total_w = over->width;
  if (under && under->width > total_w) total_w = under->width;
  if (node->children[0] && node->children[0]->type == MJX_NODE_MO &&
      mjx_node_get_attr(node->children[0], "stretchy")) {
    mjx_box* stretched = make_stretched_arrow(ctx, node->children[0],
                                              total_w + saved * 0.6);
    if (stretched) {
      mjx_box_destroy(base);
      base = stretched;
      total_w = base->width;
    }
  }

  if (over) {
    double over_x = (total_w - over->width) / 2.0;
    if (normal_accent) {
      double base_x = (total_w - base->width) / 2.0;
      over_x = base_x + base_top_accent_x(ctx, base) - over->width / 2.0;
      if (over_x < 0.0) over_x = 0.0;
      if (over_x + over->width > total_w) over_x = total_w - over->width;
    }
    double over_y;
    if (accent_mode) {
      int dot_accent = normal_accent &&
        (current_accent_cp == 0x02D9 || current_accent_cp == 0x00A8);
      over_y = dot_accent
        ? -(base->height - over->height * 0.75 + upper_gap)
        : -(base->height + upper_gap + over->depth);
    } else {
      over_y = -(base->height + upper_gap + over->depth);
    }
    mjx_box_add_child(result, over, over_x, over_y);
  }

  mjx_box_add_child(result, base, (total_w - base->width) / 2.0, 0);

  if (under) {
    double under_x = (total_w - under->width) / 2.0;
    double under_y = base->depth + lower_gap + under->height;
    mjx_box_add_child(result, under, under_x, under_y);
  }

  result->width = total_w;
  result->height = base->height;
  result->depth = base->depth;

  {
    mjx_box_child* c = result->children;
    while (c) {
      if (c->box) {
        double h = c->box->height - c->y;
        double d = c->y + c->box->depth;
        if (h > result->height) result->height = h;
        if (d > result->depth) result->depth = d;
      }
      c = c->next;
    }
  }

  return result;
}

static mjx_box* layout_table(mjx_layout_ctx* ctx, mjx_node* node, int display) {
  mjx_box* table = mjx_box_create(MJX_BOX_VBOX);
  if (!table) return NULL;

  size_t nrows = 0, ncols = 0;
  for (size_t r = 0; r < node->child_count; r++) {
    if (node->children[r]->type == MJX_NODE_MTR || node->children[r]->type == MJX_NODE_MLABELEDTR) {
      size_t cols = node->children[r]->child_count;
      if (cols > ncols) ncols = cols;
      nrows++;
    }
  }

  if (nrows == 0 || ncols == 0) return table;

  mjx_box*** cells = (mjx_box***)calloc(nrows, sizeof(mjx_box**));
  double* col_widths = (double*)calloc(ncols, sizeof(double));
  double* row_heights = (double*)calloc(nrows, sizeof(double));
  double* row_depths = (double*)calloc(nrows, sizeof(double));

  for (size_t r = 0; r < nrows; r++) {
    cells[r] = (mjx_box**)calloc(ncols, sizeof(mjx_box*));
  }

  const char* align_str = mjx_node_get_attr(node, "align");

  size_t row_idx = 0;
  for (size_t r = 0; r < node->child_count; r++) {
    mjx_node* row_node = node->children[r];
    if (row_node->type != MJX_NODE_MTR && row_node->type != MJX_NODE_MLABELEDTR) continue;
    for (size_t c = 0; c < row_node->child_count && c < ncols; c++) {
      mjx_box* cell = mjx_layout_node(ctx, row_node->children[c], display);
      cells[row_idx][c] = cell;
      if (cell) {
        if (cell->width > col_widths[c]) col_widths[c] = cell->width;
        if (cell->height > row_heights[row_idx]) row_heights[row_idx] = cell->height;
        if (cell->depth > row_depths[row_idx]) row_depths[row_idx] = cell->depth;
      }
    }
    row_idx++;
  }

  double col_space = ctx->font_size * 1.0;
  const char* col_space_attr = mjx_node_get_attr(node, "col_space");
  if (col_space_attr) col_space = atof(col_space_attr) * ctx->font_size;

  double row_space = ctx->font_size * 0.35;
  const char* row_space_attr = mjx_node_get_attr(node, "row_space");
  if (row_space_attr) row_space = atof(row_space_attr) * ctx->font_size;

  mjx_box** row_boxes = (mjx_box**)calloc(nrows, sizeof(mjx_box*));
  double table_width = 0.0;
  for (size_t r = 0; r < nrows; r++) {
    mjx_box* row_box = mjx_box_create(MJX_BOX_HBOX);
    if (!row_box) continue;

    double x = 0;
    for (size_t c = 0; c < ncols; c++) {
      mjx_box* cell = cells[r][c];
      if (!cell) { x += col_widths[c] + col_space; continue; }

      int col_align = table_col_align(align_str, c, ncols);
      double cell_x;
      if (col_align == 2) cell_x = x + col_widths[c] - cell->width;
      else if (col_align == 1) cell_x = x + (col_widths[c] - cell->width) / 2.0;
      else cell_x = x;

      mjx_box_add_child(row_box, cell, cell_x, 0);
      x += col_widths[c] + col_space;
    }

    if (ncols > 0 && x >= col_space) x -= col_space;
    row_box->width = x;
    row_box->height = row_heights[r];
    row_box->depth = row_depths[r];
    row_boxes[r] = row_box;
    if (x > table_width) table_width = x;
  }

  double total_height = 0.0;
  for (size_t r = 0; r < nrows; r++) {
    total_height += row_heights[r] + row_depths[r];
    if (r + 1 < nrows) total_height += row_space;
  }
  double scale = (ctx->font && ctx->font->em_size > 0) ?
    ctx->font_size / ctx->font->em_size : 1.0;
  double axis = ctx->mc ? ctx->mc->axis_height * scale : ctx->font_size * 0.25;
  table->width = table_width;
  table->height = total_height / 2.0 + axis;
  table->depth = total_height / 2.0 - axis;
  if (table->depth < 0.0) {
    table->height += table->depth;
    table->depth = 0.0;
  }

  double baseline_y = -table->height;
  for (size_t r = 0; r < nrows; r++) {
    if (!row_boxes[r]) continue;
    baseline_y += row_heights[r];
    double row_x = (table_width - row_boxes[r]->width) / 2.0;
    mjx_box_add_child(table, row_boxes[r], row_x, baseline_y);
    baseline_y += row_depths[r] + row_space;
  }

  for (size_t r = 0; r < nrows; r++) free(cells[r]);
  free(cells);
  free(row_boxes);
  free(col_widths);
  free(row_heights);
  free(row_depths);

  return table;
}
