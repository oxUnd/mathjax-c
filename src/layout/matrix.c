#include "layout.h"
#include "font.h"
#include "font_data.h"
#include "render.h"
#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static uint32_t decode_delim_codepoint(const char* delim) {
  const char* actual = delim;
  if (!actual || !actual[0] || actual[0] == '.') return 0;
  if (actual[0] == '\\') actual++;
  if (!actual[0]) return 0;

  {
    const char* unicode = mjx_lookup_delimiter(actual);
    if (unicode && unicode[0]) actual = unicode;
  }

  if ((unsigned char)actual[0] < 0x80) return (unsigned char)actual[0];
  if (((unsigned char)actual[0] & 0xE0) == 0xC0 && actual[1]) {
    return (((unsigned char)actual[0] & 0x1F) << 6) |
           ((unsigned char)actual[1] & 0x3F);
  }
  if (((unsigned char)actual[0] & 0xF0) == 0xE0 && actual[1] && actual[2]) {
    return (((unsigned char)actual[0] & 0x0F) << 12) |
           (((unsigned char)actual[1] & 0x3F) << 6) |
           ((unsigned char)actual[2] & 0x3F);
  }
  return (unsigned char)actual[0];
}

static void set_symmetric_delim_metrics(mjx_layout_ctx* ctx, mjx_box* box,
                                        double total_height, double axis) {
  if (!box) return;
  box->height = total_height / 2.0 + axis;
  box->depth = total_height / 2.0 - axis;
  if (box->depth < 0.0) {
    box->height += box->depth;
    box->depth = 0.0;
  }
}

static mjx_box* make_stix_delim_variant(mjx_layout_ctx* ctx, uint32_t cp,
                                        double target_height, double axis,
                                        double tolerance) {
  const unsigned int* glyphs = NULL;
  unsigned int count = 0;
  if (!ctx || !ctx->font || !mjx_stix_get_delimiter_variants(cp, &glyphs, &count)) return NULL;

  double scale = (ctx->font->em_size > 0.0) ? ctx->font_size / ctx->font->em_size : 1.0;
  unsigned int best_gid = 0;
  double best_w = 0.0;
  double best_h = 0.0;

  for (unsigned int i = 0; i < count; i++) {
    double h = mjx_font_glyph_height(ctx->font, glyphs[i]) * scale;
    double w = mjx_font_glyph_width(ctx->font, glyphs[i]) * scale;
    if (h <= 0.0 || w <= 0.0) continue;
    best_gid = glyphs[i];
    best_w = w;
    best_h = h;
    if (target_height <= h * tolerance) break;
  }
  if (!best_gid) return NULL;

  mjx_box* glyph = mjx_box_create(MJX_BOX_GLYPH);
  if (!glyph) return NULL;
  glyph->codepoint = cp;
  glyph->glyph_id = best_gid;
  glyph->font_size = ctx->font_size;
  glyph->width = best_w;
  set_symmetric_delim_metrics(ctx, glyph, best_h, axis);
  return glyph;
}

static int is_brace_codepoint(uint32_t cp) {
  return cp == '{' || cp == '}';
}

static mjx_box* make_delim_assembly(mjx_layout_ctx* ctx, uint32_t cp,
                                    double target_height, double axis,
                                    double scale) {
  mjx_glyph_part parts[32];
  unsigned int np = 0;
  double ic = 0;
  int has_parts = mjx_font_get_parts(ctx->font, cp, parts, 32, &np, &ic);

  if (!has_parts || np == 0) return NULL;

  double total_adv = 0;
  double extender_adv = 0;
  int has_ext = 0;
  for (unsigned int i = 0; i < np; i++) {
    total_adv += parts[i].full_advance;
    if (parts[i].is_extender) {
      extender_adv = parts[i].full_advance;
      has_ext = 1;
    }
  }

  int repeat_count = 0;
  if (has_ext && extender_adv > 0) {
    double base = total_adv - extender_adv;
    if (target_height > base) {
      repeat_count = (int)ceil((target_height - base) / extender_adv);
    }
  }

  mjx_box* vbox = mjx_box_create(MJX_BOX_VBOX);
  double y = 0;
  double actual_total = 0;
  for (unsigned int i = 0; i < np; i++) {
    int repeat = 1;
    if (parts[i].is_extender) repeat = repeat_count > 0 ? repeat_count : 1;

    for (int r = 0; r < repeat; r++) {
      mjx_box* part = mjx_box_create(MJX_BOX_GLYPH);
      if (!part) continue;
      part->glyph_id = parts[i].glyph_id;
      part->font_size = ctx->font_size;
      part->width = mjx_font_glyph_width(ctx->font, parts[i].glyph_id) * scale;
      part->height = mjx_font_glyph_height(ctx->font, parts[i].glyph_id) * scale;
      part->depth = 0;
      mjx_box_add_child(vbox, part, 0, y - part->height);
      {
        double adv = parts[i].full_advance * scale;
        y -= adv;
        actual_total += adv;
      }
    }
  }

  double part_w = 0;
  mjx_box_child* c = vbox->children;
  while (c) {
    if (c->box && c->box->width > part_w) part_w = c->box->width;
    c = c->next;
  }
  if (part_w <= 0) part_w = ctx->font_size * 0.5;
  vbox->width = part_w;
  if (actual_total <= 0.0) actual_total = target_height;
  set_symmetric_delim_metrics(ctx, vbox, actual_total, axis);
  return vbox;
}

static mjx_box* make_stretchy_delim(mjx_layout_ctx* ctx, const char* delim,
                                     double target_height, double axis) {
  if (!delim || !delim[0] || delim[0] == '.') return NULL;

  uint32_t cp = decode_delim_codepoint(delim);
  if (!cp) return NULL;
  double scale = (ctx->font && ctx->font->em_size > 0) ?
    ctx->font_size / ctx->font->em_size : 1.0;

  if (is_brace_codepoint(cp)) {
    mjx_box* assembled = make_delim_assembly(ctx, cp, target_height, axis, scale);
    if (assembled) return assembled;
    mjx_box* stix_variant = make_stix_delim_variant(ctx, cp, target_height, axis, 1.35);
    if (stix_variant) return stix_variant;
  }

  mjx_glyph_variant variants[16];
  unsigned int nvariants = 0;
  int has_variants = mjx_font_get_variants(ctx->font, cp, variants, 16, &nvariants);

  if (has_variants && nvariants > 0) {
    unsigned int best = 0;
    for (unsigned int i = 0; i < nvariants; i++) {
      if (variants[i].height >= target_height) {
        best = i;
        break;
      }
      best = i;
    }

    mjx_box* glyph = mjx_box_create(MJX_BOX_GLYPH);
    if (glyph) {
      glyph->codepoint = cp;
      glyph->glyph_id = variants[best].glyph_id;
      glyph->font_size = ctx->font_size;
      glyph->width = variants[best].advance_width * scale;
      {
        double actual_height = variants[best].height * scale;
        if (actual_height <= 0.0) actual_height = target_height;
        set_symmetric_delim_metrics(ctx, glyph, actual_height, axis);
      }
      return glyph;
    }
  }

  {
    mjx_box* assembled = make_delim_assembly(ctx, cp, target_height, axis, scale);
    if (assembled) return assembled;
  }

  {
    mjx_box* stix_variant = make_stix_delim_variant(ctx, cp, target_height, axis, 1.15);
    if (stix_variant) return stix_variant;
  }

  mjx_box* glyph = mjx_box_create_glyph(ctx, cp);
  if (glyph) {
    double natural = glyph->height + glyph->depth;
    if (natural > 0.0 && target_height > natural) {
      double factor = target_height / natural;
      glyph->width *= factor;
      glyph->font_size *= factor;
    }
    set_symmetric_delim_metrics(ctx, glyph, target_height, axis);
  }
  return glyph;
}

static mjx_box* make_fixed_delim(mjx_layout_ctx* ctx, const char* delim) {
  if (!delim || !delim[0] || delim[0] == '.') return NULL;

  uint32_t cp = decode_delim_codepoint(delim);
  if (!cp) return NULL;
  double scale = (ctx->font && ctx->font->em_size > 0) ?
    ctx->font_size / ctx->font->em_size : 1.0;

  mjx_box* glyph = mjx_box_create_glyph(ctx, cp);
  if (!glyph) return NULL;

  double target = ctx->font_size * 1.18;
  double natural = glyph->height + glyph->depth;
  mjx_glyph_variant variants[16];
  unsigned int nvariants = 0;
  if (mjx_font_get_variants(ctx->font, cp, variants, 16, &nvariants) && nvariants > 0) {
    unsigned int best = 0;
    for (unsigned int i = 0; i < nvariants; i++) {
      best = i;
      if (variants[i].height >= target) break;
    }
    if (variants[best].height > natural) {
      mjx_box_destroy(glyph);
      glyph = mjx_box_create(MJX_BOX_GLYPH);
      if (!glyph) return NULL;
      glyph->codepoint = cp;
      glyph->glyph_id = variants[best].glyph_id;
      glyph->font_size = ctx->font_size;
      glyph->width = variants[best].advance_width * scale;
      glyph->height = variants[best].height * scale;
      glyph->depth = 0;
    }
  }
  {
    mjx_box* stix_variant = make_stix_delim_variant(ctx, cp, target, 0.0, 1.15);
    if (stix_variant && stix_variant->height + stix_variant->depth > natural) {
      mjx_box_destroy(glyph);
      return stix_variant;
    }
    if (stix_variant) mjx_box_destroy(stix_variant);
  }
  return glyph;
}

mjx_box* mjx_layout_delimited(mjx_layout_ctx* ctx, mjx_box* inner,
                              const char* left_delim, const char* right_delim,
                              int fixed_size) {
  mjx_box* result = mjx_box_create(MJX_BOX_HBOX);
  if (!result) {
    if (inner) mjx_box_destroy(inner);
    return NULL;
  }
  result->tex_class = MJX_TEXCLASS_INNER;

  double scale = (ctx->font && ctx->font->em_size > 0) ?
    ctx->font_size / ctx->font->em_size : 1.0;
  double axis = ctx->mc ? ctx->mc->axis_height * scale : ctx->font_size * 0.25;
  double target_h = ctx->font_size;
  if (inner) {
    target_h = fixed_size ? (inner->height + inner->depth)
                          : 2.0 * fmax(inner->height - axis, inner->depth + axis);
  }
  if (target_h < ctx->font_size * 0.5) target_h = ctx->font_size;

  double x = 0;
  double inner_y = 0.0;
  mjx_box* left = NULL;
  mjx_box* right = NULL;

  if (left_delim && left_delim[0]) {
    left = fixed_size ? make_fixed_delim(ctx, left_delim)
                      : make_stretchy_delim(ctx, left_delim, target_h, axis);
  }
  if (right_delim && right_delim[0]) {
    right = fixed_size ? make_fixed_delim(ctx, right_delim)
                       : make_stretchy_delim(ctx, right_delim, target_h, axis);
  }

  if (fixed_size && inner) {
    mjx_box* ref = left ? left : right;
    if (ref) {
      double delim_center = (ref->depth - ref->height) / 2.0;
      double inner_center = (inner->depth - inner->height) / 2.0;
      inner_y = delim_center - inner_center;
    }
  }

  if (left) {
    mjx_box_add_child(result, left, x, 0);
    x += fixed_size ? (left->width + ctx->font_size * 0.15)
                    : (left->width * 0.95 + ctx->font_size * 0.08);
  }

  if (inner) {
    mjx_box_add_child(result, inner, x, inner_y);
    x += inner->width;
  }

  if (right) {
    x += fixed_size ? ctx->font_size * 0.15
                    : (ctx->font_size * 0.08 - right->width * 0.05);
    mjx_box_add_child(result, right, x, 0);
    x += right->width;
  }

  mjx_box_hpack(result);
  return result;
}

mjx_box* mjx_layout_delimited_matrix(mjx_layout_ctx* ctx, mjx_node* table,
                                      const char* left_delim, const char* right_delim,
                                      int display) {
  mjx_box* table_box = NULL;
  if (table) table_box = mjx_layout_node(ctx, table, display);
  return mjx_layout_delimited(ctx, table_box, left_delim, right_delim, 0);
}
