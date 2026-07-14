#include "layout.h"
#include "font.h"
#include "font_data.h"
#include <math.h>

/*
 * Layout for msqrt and mroot (radicals).
 * Uses MATH table metrics for radical placement.
 */

static int get_stix_radical_variant(mjx_font* font, unsigned int base_gid,
                                    double font_scale, double target_total,
                                    double* width, double* height,
                                    double* depth, unsigned int* glyph_id) {
  const unsigned int* stix_sqrt_variants = NULL;
  unsigned int stix_sqrt_count = 0;
  if (!font || !width || !height || !depth || !glyph_id) return 0;
  if (!mjx_stix_get_radical_variants(base_gid, &stix_sqrt_variants, &stix_sqrt_count)) return 0;

  unsigned int best_gid = stix_sqrt_variants[0];
  double best_width = 0.0;
  double best_height = 0.0;
  double best_depth = 0.0;
  double best_total = 0.0;

  for (unsigned int i = 0; i < stix_sqrt_count; i++) {
    unsigned int gid = stix_sqrt_variants[i];
    mjx_glyph_id_info info;
    if (!mjx_font_get_glyph_id_info(font, gid, &info)) continue;
    double w = info.advance_width * font_scale;
    double h = info.height * font_scale;
    double d = info.depth * font_scale;
    double total = h + d;
    if (total <= 0.0 || w <= 0.0) continue;
    best_gid = gid;
    best_width = w;
    best_height = h;
    best_depth = d;
    best_total = total;
    if (target_total <= total * 1.08) break;
  }

  if (best_total <= 0.0 || best_width <= 0.0) return 0;
  *glyph_id = best_gid;
  *width = best_width;
  *height = best_height;
  *depth = best_depth;
  return 1;
}

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

  double surd_extra = (display ? mc->radical_display_style_vertical_gap
                               : mc->radical_vertical_gap) * scale;
  if (surd_extra <= 0.0) surd_extra = fmax(radical_rule, ctx->font_size * 0.06);
  double p = display ? ctx->font->x_height * scale : radical_rule;
  double surd_target = radicand->height + radicand->depth +
                       radical_rule + surd_extra + p / 4.0;
  double radical_total = radical_height + radical_depth;

  /* Check for stretchy radical variant */
  int found_variant = 0;
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
        {
          mjx_glyph_id_info info;
          if (mjx_font_get_glyph_id_info(ctx->font, radical_glyph_id, &info)) {
            radical_width = info.advance_width * scale;
            radical_height = info.height * scale;
            radical_depth = info.depth * scale;
            radical_total = radical_height + radical_depth;
            found_variant = 1;
          }
        }
      }
    }
  }
  if (!found_variant) {
    double variant_width = 0.0;
    double variant_height = 0.0;
    double variant_depth = 0.0;
    unsigned int variant_gid = 0;
    if (get_stix_radical_variant(ctx->font, radical_glyph_id, scale, surd_target,
                                 &variant_width, &variant_height,
                                 &variant_depth, &variant_gid)) {
      radical_glyph_id = variant_gid;
      radical_width = variant_width;
      radical_height = variant_height;
      radical_depth = variant_depth;
      radical_total = radical_height + radical_depth;
      found_variant = 1;
    }
  }

  if (surd_target > radical_total) {
    double stretch = radical_total > 0.0 ? surd_target / radical_total : 1.0;
    radical_width *= stretch;
    radical_total = surd_target;
    radical_depth *= stretch;
    radical_height = fmax(0.0, radical_total - radical_depth);
  }

  double q;
  if (radicand->height + radicand->depth > radical_total) {
    q = (radicand->height + radicand->depth -
         (radical_total - radical_rule - surd_extra - p / 2.0)) / 2.0;
  } else {
    q = surd_extra + p / 4.0;
  }
  if (q < radical_rule) q = radical_rule;

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

  /* Layout the degree using its inherited MathJax scriptlevel. */
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
      return result;
    }
  }

  return base;
}
