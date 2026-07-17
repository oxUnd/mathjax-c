#include "layout.h"
#include "font.h"
#include <math.h>
#include <string.h>

static uint32_t decode_first_codepoint(const char* text, size_t len) {
  if (!text || len == 0) return 0;
  unsigned char c = (unsigned char)text[0];
  if ((c & 0x80) == 0) return c;
  if ((c & 0xE0) == 0xC0 && len >= 2) {
    return ((c & 0x1F) << 6) | ((unsigned char)text[1] & 0x3F);
  }
  if ((c & 0xF0) == 0xE0 && len >= 3) {
    return ((c & 0x0F) << 12) |
           (((unsigned char)text[1] & 0x3F) << 6) |
           ((unsigned char)text[2] & 0x3F);
  }
  if ((c & 0xF8) == 0xF0 && len >= 4) {
    return ((c & 0x07) << 18) |
           (((unsigned char)text[1] & 0x3F) << 12) |
           (((unsigned char)text[2] & 0x3F) << 6) |
           ((unsigned char)text[3] & 0x3F);
  }
  return c;
}

static int is_integral_codepoint(uint32_t cp) {
  return (cp >= 0x222B && cp <= 0x2233) || cp == 0x2A0C;
}

static int is_integral_script_base(mjx_node* node) {
  if (!node) return 0;
  if (node->type == MJX_NODE_MO) {
    return is_integral_codepoint(decode_first_codepoint(node->text, node->text_len));
  }
  if (node->type == MJX_NODE_MROW && node->child_count > 0) {
    for (size_t i = node->child_count; i > 0; i--) {
      mjx_node* child = node->children[i - 1];
      if (!child || child->type == MJX_NODE_MSPACE) continue;
      return is_integral_script_base(child);
    }
  }
  return 0;
}

static int box_visual_right(mjx_layout_ctx* ctx, mjx_box* box, double* right) {
  if (!ctx || !box || !right) return 0;

  if (box->type == MJX_BOX_HBOX || box->type == MJX_BOX_VBOX ||
      box->type == MJX_BOX_ATOM) {
    int found = 0;
    double max_right = 0.0;
    for (mjx_box_child* child = box->children; child; child = child->next) {
      double child_right = 0.0;
      if (!child->box || !box_visual_right(ctx, child->box, &child_right)) continue;
      child_right += child->x;
      if (!found || child_right > max_right) max_right = child_right;
      found = 1;
    }
    if (found) {
      *right = max_right;
      return 1;
    }
    return 0;
  }

  if (box->type != MJX_BOX_GLYPH) return 0;

  mjx_font* font = (box->font_index == 1 && ctx->fallback_font) ? ctx->fallback_font : ctx->font;
  if (!font) return 0;

  double scale = (font->em_size > 0.0) ? box->font_size / font->em_size : 1.0;
  if (box->glyph_id) {
    mjx_glyph_id_info info;
    if (!mjx_font_get_glyph_id_info(font, box->glyph_id, &info)) return 0;
    *right = info.bbox_right * scale;
    return 1;
  }

  if (box->codepoint) {
    mjx_glyph_info info;
    if (!mjx_font_get_glyph(font, box->codepoint, &info)) return 0;
    *right = info.bbox_right * scale;
    return 1;
  }

  return 0;
}

static int is_default_limits_base(mjx_node* node) {
  if (!node) return 0;
  const char* movablelimits = mjx_node_get_attr(node, "movablelimits");
  if (movablelimits && strcmp(movablelimits, "true") == 0) return 1;
  if (node->type != MJX_NODE_MO) return 0;
  if (node->mo_movablelimits) return 1;
  if (!node->mo_largeop) return 0;
  return !is_integral_codepoint(decode_first_codepoint(node->text, node->text_len));
}

static mjx_box* layout_scripts_as_limits(mjx_layout_ctx* ctx,
                                         mjx_box* base,
                                         mjx_box* sub,
                                         mjx_box* sup,
                                         int display) {
  mjx_math_constants* mc = &ctx->font->math_constants;
  double saved = ctx->font_size;
  double upper_gap = (display && mc) ? mc->upper_limit_gap_min : (saved * 0.2);
  double lower_gap = (display && mc) ? mc->lower_limit_gap_min : (saved * 0.2);
  double width = base->width;
  if (sup && sup->width > width) width = sup->width;
  if (sub && sub->width > width) width = sub->width;

  mjx_box* result = mjx_box_create(MJX_BOX_VBOX);
  if (!result) return base;
  result->tex_class = base->tex_class;

  if (sup) {
    double sup_x = (width - sup->width) / 2.0;
    double sup_y = -(base->height + upper_gap + sup->depth);
    mjx_box_add_child(result, sup, sup_x, sup_y);
  }

  mjx_box_add_child(result, base, (width - base->width) / 2.0, 0);

  if (sub) {
    double sub_x = (width - sub->width) / 2.0;
    double sub_y = base->depth + lower_gap + sub->height;
    mjx_box_add_child(result, sub, sub_x, sub_y);
  }

  result->width = width;
  result->height = base->height;
  result->depth = base->depth;

  for (mjx_box_child* c = result->children; c; c = c->next) {
    if (!c->box) continue;
    double h = c->box->height - c->y;
    double d = c->y + c->box->depth;
    if (h > result->height) result->height = h;
    if (d > result->depth) result->depth = d;
  }

  return result;
}

/* 
 * Layout for msup, msub, msubsup nodes.
 * Follows TeX algorithm for script placement.
 */

mjx_box* mjx_layout_scripts(mjx_layout_ctx* ctx, mjx_node* node, int display) {
  mjx_math_constants* mc = &ctx->font->math_constants;

  if (node->child_count < 2) {
    /* No base or scripts, just layout what we have */
    if (node->child_count == 0) return mjx_box_create(MJX_BOX_HBOX);
    return mjx_layout_node(ctx, node->children[0], display);
  }

  mjx_box* base = mjx_layout_node(ctx, node->children[0], display);
  if (!base) return NULL;

  mjx_box* sub = NULL;
  mjx_box* sup = NULL;

  ctx->script_depth++;

  /* Determine which children are subscript and superscript */
  if (node->type == MJX_NODE_MSUP) {
    sup = mjx_layout_node(ctx, node->children[1], 0);
  } else if (node->type == MJX_NODE_MSUB) {
    sub = mjx_layout_node(ctx, node->children[1], 0);
  } else if (node->type == MJX_NODE_MSUBSUP) {
    sub = mjx_layout_node(ctx, node->children[1], 0);
    if (node->child_count > 2) {
      sup = mjx_layout_node(ctx, node->children[2], 0);
    }
  }

  ctx->script_depth--;

  if (display && !mjx_node_get_attr(node, "nolimits") &&
      is_default_limits_base(node->children[0])) {
    return layout_scripts_as_limits(ctx, base, sub, sup, display);
  }

  /* Calculate script positions */
  double sub_shift = mc->subscript_shift_down;
  double sup_shift = display ? mc->superscript_shift_up : mc->superscript_shift_up_cramped;
  int integral_base = display && is_integral_script_base(node->children[0]);

  mjx_box* result = mjx_box_create(MJX_BOX_HBOX);
  if (!result) {
    if (sub) mjx_box_destroy(sub);
    if (sup) mjx_box_destroy(sup);
    return base;
  }
  result->tex_class = base->tex_class;

  double x = 0;

  /* Place the base */
  mjx_box_add_child(result, base, x, 0);

  double base_italic_corr = 0;
  /* Get italic correction from last glyph in base */
  {
    mjx_box* last = base;
    mjx_box_child* c = base->children;
    while (c && c->next) c = c->next;
    if (c && c->box) last = c->box;
    base_italic_corr = last->italic_correction;
  }

  if (sup) {
    double min_sup_shift = base->height - mc->superscript_baseline_drop_max;
    if (sup_shift < min_sup_shift) sup_shift = min_sup_shift;
    {
      double sup_bottom_min = mc->superscript_bottom_min + sup->depth;
      if (sup_shift < sup_bottom_min) sup_shift = sup_bottom_min;
    }
  }

  if (sub) {
    double min_sub_shift = base->depth + mc->subscript_baseline_drop_min;
    if (sub_shift < min_sub_shift) sub_shift = min_sub_shift;
    {
      double sub_top_limit = mc->subscript_top_max;
      double needed = sup ? 0.0 : (sub->height - sub_top_limit);
      if (needed > sub_shift) sub_shift = needed;
    }
  }

  if (integral_base) {
    if (sup) sup_shift = base->height * 0.48;
    if (sub) sub_shift = base->depth * 0.40;
  }

  double sup_y = -sup_shift;
  double sub_y = sub_shift;

  if (sup && sub) {
    double gap = sub_y - sub->height - (sup_y + sup->depth);
    if (gap < mc->sub_superscript_gap_min) {
      sub_y += mc->sub_superscript_gap_min - gap;
    }

    if (!integral_base) {
      double bottom = -(sup_y + sup->depth);
      if (bottom > mc->superscript_bottom_max_with_subscript) {
        sup_y -= bottom - mc->superscript_bottom_max_with_subscript;
      }
    }
  }

  double script_x = x + base->width;
  double sup_x = script_x + base_italic_corr;
  double sub_x = script_x;
  if (integral_base) {
    double visual_right = 0.0;
    if (box_visual_right(ctx, base, &visual_right)) {
      double integral_right = x + visual_right;
      sup_x = integral_right - ctx->font_size * 0.02 + base_italic_corr;
      sub_x = integral_right - ctx->font_size * 0.18;
    }
  }

  if (sup) {
    mjx_box_add_child(result, sup, sup_x, sup_y);
  }

  if (sub) {
    mjx_box_add_child(result, sub, sub_x, sub_y);
  }

  result->width = base->width;
  if (sup && sup_x + sup->width > result->width) result->width = sup_x + sup->width;
  if (sub && sub_x + sub->width > result->width) result->width = sub_x + sub->width;
  result->width += mc->space_after_script;

  result->height = base->height;
  if (sup) {
    double sup_height = sup->height - sup_y;
    if (sup_height > result->height) result->height = sup_height;
  }

  result->depth = base->depth;
  if (sub) {
    double sub_depth = sub_y + sub->depth;
    if (sub_depth > result->depth) result->depth = sub_depth;
  }

  return result;
}
