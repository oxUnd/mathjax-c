#include "layout.h"
#include "font.h"
#include <math.h>

/*
 * Layout for msqrt and mroot (radicals).
 * Uses MATH table metrics for radical placement.
 */

mjx_box* mjx_layout_sqrt(mjx_layout_ctx* ctx, mjx_node* node, int display) {
  mjx_math_constants* mc = &ctx->font->math_constants;
  double scale = (ctx->font && ctx->font->em_size > 0) ?
    ctx->font_size / ctx->font->em_size : 1.0;
  double radical_rule = mc->radical_rule_thickness * scale;
  if (radical_rule < 0.5) radical_rule = 0.5;

  /* Layout the radicand (first child or all children as a row) */
  mjx_box* radicand;
  if (node->type == MJX_NODE_MROOT && node->child_count >= 2) {
    /* For mroot, first child is radicand, rest are degree */
    radicand = mjx_layout_node(ctx, node->children[0], display);
  } else {
    radicand = mjx_layout_row(ctx, node->children, node->child_count, display);
  }

  if (!radicand) return mjx_box_create(MJX_BOX_HBOX);

  uint32_t radical_cp = 0x221A;  /* √ */

  mjx_glyph_info radical_info;
  double radical_width = 0;
  double radical_height = 0;
  double radical_depth = 0;
  unsigned int radical_glyph_id = 0;

  if (mjx_font_get_glyph(ctx->font, radical_cp, &radical_info)) {
    radical_width = radical_info.advance_width * scale;
    radical_glyph_id = radical_info.glyph_id;
    radical_height = radical_info.height * scale;
    radical_depth = radical_info.depth * scale;
  } else {
    radical_width = ctx->font_size * 1.2;
    radical_height = ctx->font_size * 0.8;
    radical_depth = ctx->font_size * 0.2;
  }

  double surd_extra = fmax(radical_rule, ctx->font_size * 0.06);
  double p = display ? ctx->font->x_height * scale : ctx->font_size * 0.12;
  double surd_target = radicand->height + radicand->depth +
                       radical_rule + surd_extra + p / 4.0;
  double radical_total = radical_height + radical_depth;

  /* Check for stretchy radical variant */
  {
    mjx_glyph_variant variants[16];
    unsigned int var_count = 0;
    if (mjx_font_get_variants(ctx->font, radical_cp, variants, 16, &var_count)) {
      if (var_count > 0) {
        unsigned int best = var_count - 1;
        for (unsigned int i = 0; i < var_count; i++) {
          double vh = variants[i].height * scale;
          if (vh >= surd_target) {
            best = i;
            break;
          }
        }
        radical_glyph_id = variants[best].glyph_id;
        radical_total = variants[best].height * scale;
        radical_width = variants[best].advance_width * scale;
        radical_height = fmax(0.0, radical_total - radical_depth);
      }
    }
  }

  if (surd_target > radical_total) {
    double stretch = radical_total > 0.0 ? surd_target / radical_total : 1.0;
    radical_width *= stretch;
    radical_total = surd_target;
    radical_depth = radicand->depth;
    radical_height = fmax(0.0, radical_total - radical_depth);
  }

  double q;
  if (radicand->height + radicand->depth > radical_total) {
    q = (radicand->height + radicand->depth -
         (radical_total - radical_rule - surd_extra - p / 2.0)) / 2.0;
  } else {
    q = surd_extra + p / 4.0;
  }
  {
    double min_clearance = ctx->font_size * 0.20;
    if (q < min_clearance) q = min_clearance;
  }
  q += ctx->font_size * 0.06;

  double H = radicand->height + q + radical_rule;

  /* Build the radical box */
  mjx_box* result = mjx_box_create(MJX_BOX_HBOX);
  if (!result) {
    mjx_box_destroy(radicand);
    return NULL;
  }

  double x = 0;
  double rule_overlap = fmax(radical_rule, ctx->font_size * 0.03);
  double rule_x = fmax(0.0, radical_width - rule_overlap);
  double rule_width = radicand->width + rule_overlap;
  double rule_y = -H;

  mjx_box* radical_box = mjx_box_create(MJX_BOX_GLYPH);
  if (radical_box) {
    radical_box->codepoint = radical_cp;
    radical_box->glyph_id = radical_glyph_id;
    radical_box->font_size = ctx->font_size;
    radical_box->width = radical_width;
    radical_box->height = radical_height;
    radical_box->depth = radical_depth;
    mjx_box_add_child(result, radical_box, x, radical_height - H);
    x += radical_width;
  }

  /* Add the radicand */
  mjx_box_add_child(result, radicand, x, 0);

  /* Add overline (rule above radicand) */
  mjx_box* overline = mjx_box_create_rule(rule_width, radical_rule, 0);
  if (overline) {
    overline->height = 0;
    overline->depth = radical_rule;
    mjx_box_add_child(result, overline, rule_x, rule_y);
  }

  mjx_box_hpack(result);
  return result;
}

mjx_box* mjx_layout_root(mjx_layout_ctx* ctx, mjx_node* node, int display) {
  /* mroot: first child = base, second child = degree */
  if (node->child_count < 2) {
    return mjx_layout_sqrt(ctx, node, display);
  }

  mjx_box* base = mjx_layout_sqrt(ctx, node, display);
  if (!base) return mjx_box_create(MJX_BOX_HBOX);

  mjx_math_constants* mc = &ctx->font->math_constants;
  double scale = (ctx->font && ctx->font->em_size > 0) ?
    ctx->font_size / ctx->font->em_size : 1.0;
  double degree_bottom_raise = mc->radical_degree_bottom_raise_percent;
  double kern_after = mc->radical_kern_after_degree * scale;

  /* Layout the degree in script style */
  double saved = ctx->font_size;
  ctx->font_size *= mjx_font_script_scale(ctx->font, 1);
  ctx->script_depth++;

  mjx_box* degree = mjx_layout_node(ctx, node->children[1], display);
  ctx->script_depth--;

  if (degree) {
    double radical_width = ctx->font_size * 0.8;
    if (base->children && base->children->box) {
      radical_width = base->children->box->width;
    }

    double base_x = fmax(degree->width * 0.25, radical_width * 0.18);
    double degree_x = fmax(0.0, base_x - degree->width * 0.75);
    double degree_y = -fmax(base->height * degree_bottom_raise, degree->height * 0.6) + degree->depth;

    mjx_box* result = mjx_box_create(MJX_BOX_HBOX);
    if (result) {
      if (base_x > radical_width * 0.55 + kern_after) {
        base_x = radical_width * 0.55 + kern_after;
      }

      mjx_box_add_child(result, degree, degree_x, degree_y);
      mjx_box_add_child(result, base, base_x, 0);
      mjx_box_hpack(result);
      ctx->font_size = saved;
      return result;
    }
  }

  ctx->font_size = saved;
  return base;
}
