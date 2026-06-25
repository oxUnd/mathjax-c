#include "layout.h"
#include "font.h"
#include "font_data.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*
 * Layout for mfrac (fractions).
 * Follows TeX algorithm using MATH table metrics.
 */

static mjx_box* make_fraction_rule_glyph(mjx_layout_ctx* ctx, double width,
                                         double min_thickness) {
  const unsigned int* overline_glyphs = NULL;
  unsigned int overline_count = 0;
  if (!ctx || !ctx->font || width <= 0.0) return NULL;
  if (!mjx_stix_get_fraction_rule_variants(&overline_glyphs, &overline_count)) return NULL;

  double metric_scale = (ctx->font->em_size > 0.0) ? ctx->font_size / ctx->font->em_size : 1.0;
  unsigned int best_gid = 0;
  double best_w = 0.0;
  double best_h = 0.0;

  for (unsigned int i = 0; i < overline_count; i++) {
    double w = mjx_font_glyph_width(ctx->font, overline_glyphs[i]) * metric_scale;
    double h = mjx_font_glyph_height(ctx->font, overline_glyphs[i]) * metric_scale;
    if (w <= 0.0 || h <= 0.0) continue;
    best_gid = overline_glyphs[i];
    best_w = w;
    best_h = h;
    if (width <= w * 1.12) break;
  }
  if (!best_gid) return NULL;

  mjx_box* rule = mjx_box_create(MJX_BOX_GLYPH);
  if (!rule) return NULL;
  rule->glyph_id = best_gid;
  rule->font_size = ctx->font_size;
  rule->width = width;
  rule->height = fmax(best_h, min_thickness);
  rule->depth = 0.0;
  rule->bb_y = ctx->font_size * 0.77;
  if (fabs(width - best_w) > best_w * 0.08) {
    rule->allow_nonuniform_scale = 1;
  }
  return rule;
}

mjx_box* mjx_layout_fraction(mjx_layout_ctx* ctx, mjx_node* node, int display) {
  mjx_math_constants* mc = &ctx->font->math_constants;
  double saved_size = ctx->font_size;
  double scale = (ctx->font && ctx->font->em_size > 0.0) ?
    saved_size / ctx->font->em_size : 1.0;

  if (node->child_count < 2) {
    if (node->child_count == 0) return mjx_box_create(MJX_BOX_HBOX);
    return mjx_layout_node(ctx, node->children[0], display);
  }

  /* Check for bevelled fraction */
  const char* bevelled = mjx_node_get_attr(node, "bevelled");
  int is_bevelled = (bevelled && strcmp(bevelled, "true") == 0);
  const char* linethickness = mjx_node_get_attr(node, "linethickness");
  int zero_rule = 0;

  /* Get numerator and denominator */
  double num_shift, denom_shift;
  double num_gap, denom_gap;
  double rule_thickness = mc->fraction_rule_thickness * scale;
  double axis = mc->axis_height * scale;

  if (display) {
    num_shift = mc->fraction_numerator_display_style_shift_up * scale;
    denom_shift = mc->fraction_denominator_display_style_shift_down * scale;
    num_gap = mc->fraction_numerator_display_style_gap_min * scale;
    denom_gap = mc->fraction_denominator_display_style_gap_min * scale;
  } else {
    num_shift = mc->fraction_numerator_shift_up * scale;
    denom_shift = mc->fraction_denominator_shift_down * scale;
    num_gap = mc->fraction_numerator_gap_min * scale;
    denom_gap = mc->fraction_denominator_gap_min * scale;
  }

  if (linethickness) {
    if (strcmp(linethickness, "0pt") == 0 || strcmp(linethickness, "0") == 0 || strcmp(linethickness, "0em") == 0) {
      rule_thickness = 0;
      zero_rule = 1;
    } else if (strstr(linethickness, "pt")) {
      rule_thickness = atof(linethickness) * saved_size / 72.0;
    } else if (strstr(linethickness, "em")) {
      rule_thickness = atof(linethickness) * saved_size;
    } else if (linethickness[0]) {
      rule_thickness = atof(linethickness);
    }
  }

  if (!zero_rule && rule_thickness < 0.5) rule_thickness = 0.5;

  ctx->font_size = saved_size * mjx_font_script_scale(ctx->font, 1);
  mjx_box* num = mjx_layout_node(ctx, node->children[0], 0);
  mjx_box* denom = mjx_layout_node(ctx, node->children[1], 0);
  ctx->font_size = saved_size;

  if (!num || !denom) {
    if (num) mjx_box_destroy(num);
    if (denom) mjx_box_destroy(denom);
    mjx_box* empty = mjx_box_create(MJX_BOX_HBOX);
    if (num) empty = num;
    return empty;
  }

  mjx_box* result = mjx_box_create(MJX_BOX_VBOX);
  if (!result) {
    mjx_box_destroy(num);
    mjx_box_destroy(denom);
    return NULL;
  }

  double max_width = (num->width > denom->width) ? num->width : denom->width;
  if (is_bevelled) {
    max_width = num->width + denom->width + mc->skewed_fraction_horizontal_gap + saved_size * 0.25;
  }
  double half_rule = rule_thickness / 2.0;

  if (is_bevelled) {
    double hgap = mc->skewed_fraction_horizontal_gap * scale;
    double vgap = mc->skewed_fraction_vertical_gap * scale;
    if (hgap <= 0) hgap = saved_size * 0.25;
    if (vgap <= 0) vgap = saved_size * 0.15;

    mjx_box* result = mjx_box_create(MJX_BOX_HBOX);
    if (!result) {
      mjx_box_destroy(num);
      mjx_box_destroy(denom);
      return NULL;
    }

    mjx_box_add_child(result, num, 0, -vgap - num->depth);

    mjx_box* slash = mjx_box_create(MJX_BOX_DIAGONAL);
    if (slash) {
      double slash_x = num->width + hgap * 0.4;
      double slash_h = num->height + num->depth + denom->height + denom->depth + vgap * 2.0;
      slash->diag_x1 = 0;
      slash->diag_y1 = slash_h;
      slash->diag_x2 = hgap;
      slash->diag_y2 = 0;
      slash->diag_thickness = rule_thickness;
      slash->width = hgap;
      slash->height = slash_h;
      slash->depth = 0;
      mjx_box_add_child(result, slash, slash_x, -(num->height + vgap));
    }

    mjx_box_add_child(result, denom, num->width + hgap, denom->height + vgap);
    mjx_box_hpack(result);
    return result;
  }

  if (zero_rule) {
    double gap = saved_size * (display ? 0.20 : 0.16);
    double num_pos = -(num->depth + gap * 0.5);
    double denom_pos = denom->height + gap * 0.5;

    double num_x = (max_width - num->width) / 2.0;
    double denom_x = (max_width - denom->width) / 2.0;

    mjx_box_add_child(result, num, num_x, num_pos);
    mjx_box_add_child(result, denom, denom_x, denom_pos);

    result->width = max_width;
    {
      double top = num_pos - num->height;
      double bottom = denom_pos + denom->depth;
      double total = bottom - top;
      result->height = total / 2.0;
      result->depth = total / 2.0;
    }
    return result;
  }

  /* Baseline shifts for numerator and denominator. */
  if (num_shift < num->depth + half_rule + num_gap) {
    num_shift = num->depth + half_rule + num_gap;
  }
  if (denom_shift < denom->height + half_rule + denom_gap) {
    denom_shift = denom->height + half_rule + denom_gap;
  }

  double num_pos = -num_shift;
  double denom_pos = denom_shift;

  /* Center under the maximum width */
  double num_x = (max_width - num->width) / 2.0;
  double denom_x = (max_width - denom->width) / 2.0;

  /* Add numerator */
  mjx_box_add_child(result, num, num_x, num_pos);
  /* Add fraction rule when requested */
  if (rule_thickness > 0) {
    mjx_box* rule = make_fraction_rule_glyph(ctx, max_width, rule_thickness);
    if (!rule) rule = mjx_box_create_rule(max_width, rule_thickness, 0);
    if (rule) mjx_box_add_child(result, rule, (max_width - rule->width) / 2.0, -rule->height / 2.0);
  }
  /* Add denominator */
  mjx_box_add_child(result, denom, denom_x, denom_pos);

  result->width = max_width;
  result->height = num_shift + num->height;
  result->depth = denom_pos + denom->depth;

  return result;
}

/* 
 * Layout for atop, stackrel, etc. (stacks without fraction bar)
 */
mjx_box* mjx_layout_stack(mjx_layout_ctx* ctx, mjx_node** nodes, size_t count, int display) {
  if (count == 0) return mjx_box_create(MJX_BOX_HBOX);
  if (count == 1) return mjx_layout_node(ctx, nodes[0], display);

  mjx_math_constants* mc = &ctx->font->math_constants;
  double scale = (ctx->font && ctx->font->em_size > 0.0) ?
    ctx->font_size / ctx->font->em_size : 1.0;

  double top_shift = (display ? mc->stack_top_display_style_shift_up : mc->stack_top_shift_up) * scale;
  double bottom_shift = (display ? mc->stack_bottom_display_style_shift_down : mc->stack_bottom_shift_down) * scale;
  double gap = (display ? mc->stack_display_style_gap_min : mc->stack_gap_min) * scale;

  mjx_box* top = mjx_layout_node(ctx, nodes[0], display);
  mjx_box* bottom = mjx_layout_node(ctx, nodes[1], display);

  if (!top || !bottom) {
    if (top) mjx_box_destroy(top);
    if (bottom) mjx_box_destroy(bottom);
    return mjx_box_create(MJX_BOX_HBOX);
  }

  double max_width = (top->width > bottom->width) ? top->width : bottom->width;
  double axis = mc->axis_height * scale;

  /* Position top above axis */
  double top_pos = -(top_shift + top->height);
  double bottom_pos = bottom_shift;

  /* Adjust to maintain minimum gap */
  double actual_gap = bottom_pos - (top_pos + top->height);
  if (actual_gap < gap) {
    bottom_pos += (gap - actual_gap) / 2.0;
    top_pos -= (gap - actual_gap) / 2.0;
  }

  mjx_box* result = mjx_box_create(MJX_BOX_VBOX);
  if (!result) return NULL;

  double top_x = (max_width - top->width) / 2.0;
  double bottom_x = (max_width - bottom->width) / 2.0;

  mjx_box_add_child(result, top, top_x, top_pos);
  mjx_box_add_child(result, bottom, bottom_x, bottom_pos);

  result->width = max_width;
  result->height = -(top_pos) + top->height;
  result->depth = bottom_pos + bottom->depth;

  return result;
}
