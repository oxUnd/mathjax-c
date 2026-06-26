#include "render.h"
#include "layout.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void render_glyph_box(mjx_renderer* r, mjx_box* box,
                              mjx_buf* buf, double offset_x, double offset_y) {
  unsigned int w, h;
  int left, top;
  unsigned char* alpha;
  mjx_font* font = (box->font_index == 1 && r->fallback_font) ? r->fallback_font : r->font;
  double natural_w = 0;
  double natural_h = 0;
  double render_size = box->font_size > 0.0 ? box->font_size : font->em_size;
  double render_scale = (font && font->em_size > 0.0) ?
    render_size / font->em_size : 1.0;

  if (box->glyph_id != 0) {
    alpha = mjx_font_render_glyph_id_size(font, box->glyph_id, render_size, &w, &h, &left, &top);
    natural_w = mjx_font_glyph_width(font, box->glyph_id) * render_scale;
    natural_h = mjx_font_glyph_height(font, box->glyph_id) * render_scale;
  } else {
    alpha = mjx_font_render_glyph_size(font, box->codepoint, render_size, &w, &h, &left, &top);
    if (box->codepoint) {
      mjx_glyph_info info;
      if (mjx_font_get_glyph(font, box->codepoint, &info)) {
        natural_w = info.advance_width * render_scale;
        natural_h = (info.height + info.depth) * render_scale;
      }
    }
  }
  if (!alpha) return;
  if (natural_w <= 0.0 && w > 0) natural_w = (double)w;
  if (natural_h <= 0.0 && h > 0) natural_h = (double)h;

  double sx = 1.0;
  double sy = 1.0;
  if (natural_w > 0.0 && box->width > 0.0) sx = box->width / natural_w;
  if (natural_h > 0.0 && (box->height + box->depth) > 0.0) sy = (box->height + box->depth) / natural_h;
  if (sx <= 0.0) sx = 1.0;
  if (sy <= 0.0) sy = sx;
  if (!box->allow_nonuniform_scale && fabs(sx - sy) > 0.15) sx = sy;

  int x = (int)(offset_x + box->bb_x);
  int y = (int)(offset_y + box->bb_y);

  if (fabs(sx - 1.0) < 0.05 && fabs(sy - 1.0) < 0.05) {
    mjx_buf_blit_alpha(buf, alpha, x + left, y - top - (int)h, w, h, r->fg_color, 0, 0);
  } else {
    unsigned int sw = (unsigned int)fmax(1.0, round(w * sx));
    unsigned int sh = (unsigned int)fmax(1.0, round(h * sy));
    unsigned char* scaled = (unsigned char*)calloc(sw * sh, 1);
    if (scaled) {
      for (unsigned int dy = 0; dy < sh; dy++) {
        unsigned int syi = (unsigned int)fmin((double)h - 1, floor(dy / sy));
        for (unsigned int dx = 0; dx < sw; dx++) {
          unsigned int sxi = (unsigned int)fmin((double)w - 1, floor(dx / sx));
          scaled[dy * sw + dx] = alpha[syi * w + sxi];
        }
      }
      mjx_buf_blit_alpha(buf, scaled,
                         x + (int)round(left * sx),
                         y - (int)round(top * sy) - (int)sh,
                         sw, sh, r->fg_color, 0, 0);
      free(scaled);
    }
  }
  free(alpha);
}

static void render_rule_box(mjx_renderer* r, mjx_box* box,
                             mjx_buf* buf, double offset_x, double offset_y) {
  double x = offset_x + box->bb_x;
  double y = offset_y + box->bb_y;
  int h = (int)fmax(1.0, round(box->height + box->depth));
  mjx_buf_fill_rect(buf, (int)x, (int)y,
                     (int)ceil(box->width), h,
                     r->fg_color);
}

static void compute_extents(mjx_renderer* r, mjx_box* box, double ox, double oy,
                             double* min_x, double* min_y,
                             double* max_x, double* max_y) {
  if (!box) return;
  double bx = ox + box->bb_x;
  double by = oy + box->bb_y - box->height;
  double bx2 = bx + box->width;
  double by2 = by + box->height + box->depth;

  if (box->type == MJX_BOX_GLYPH) {
    unsigned int w = 0, h = 0;
    int left = 0, top = 0;
    unsigned char* alpha = NULL;
    mjx_font* font = (box->font_index == 1 && r->fallback_font) ? r->fallback_font : r->font;
    double natural_w = 0;
    double natural_h = 0;
    double render_size = box->font_size > 0.0 ? box->font_size : font->em_size;
    double render_scale = (font && font->em_size > 0.0) ?
      render_size / font->em_size : 1.0;
    if (box->glyph_id != 0) {
      alpha = mjx_font_render_glyph_id_size(font, box->glyph_id, render_size, &w, &h, &left, &top);
      natural_w = mjx_font_glyph_width(font, box->glyph_id) * render_scale;
      natural_h = mjx_font_glyph_height(font, box->glyph_id) * render_scale;
    } else {
      alpha = mjx_font_render_glyph_size(font, box->codepoint, render_size, &w, &h, &left, &top);
      if (box->codepoint) {
        mjx_glyph_info info;
        if (mjx_font_get_glyph(font, box->codepoint, &info)) {
          natural_w = info.advance_width * render_scale;
          natural_h = (info.height + info.depth) * render_scale;
        }
      }
    }
    if (alpha) {
      if (natural_w <= 0.0 && w > 0) natural_w = (double)w;
      if (natural_h <= 0.0 && h > 0) natural_h = (double)h;
      double sx = 1.0;
      double sy = 1.0;
      if (natural_w > 0.0 && box->width > 0.0) sx = box->width / natural_w;
      if (natural_h > 0.0 && (box->height + box->depth) > 0.0) sy = (box->height + box->depth) / natural_h;
      if (sx <= 0.0) sx = 1.0;
      if (sy <= 0.0) sy = sx;
      if (!box->allow_nonuniform_scale && fabs(sx - sy) > 0.15) sx = sy;
      free(alpha);
      bx = ox + box->bb_x + left * sx;
      by = oy + box->bb_y - top * sy;
      bx2 = bx + w * sx;
      by2 = by + h * sy;
    }
  }

  if (bx < *min_x) *min_x = bx;
  if (by < *min_y) *min_y = by;
  if (bx2 > *max_x) *max_x = bx2;
  if (by2 > *max_y) *max_y = by2;

  if (box->type == MJX_BOX_HBOX || box->type == MJX_BOX_VBOX || box->type == MJX_BOX_ATOM) {
    mjx_box_child* child = box->children;
    while (child) {
      compute_extents(r, child->box, ox + box->bb_x + child->x, oy + box->bb_y + child->y,
                       min_x, min_y, max_x, max_y);
      child = child->next;
    }
  }
}

static void render_box_internal(mjx_renderer* r, mjx_box* box,
                                 mjx_buf* buf, double offset_x, double offset_y) {
  if (!box || !buf) return;

  switch (box->type) {
    case MJX_BOX_GLYPH:
      render_glyph_box(r, box, buf, offset_x, offset_y);
      break;

    case MJX_BOX_RULE:
      render_rule_box(r, box, buf, offset_x, offset_y);
      break;

    case MJX_BOX_HBOX:
    case MJX_BOX_VBOX:
    case MJX_BOX_ATOM:
      {
        mjx_box_child* child = box->children;
        while (child) {
          render_box_internal(r, child->box, buf,
                               offset_x + box->bb_x + child->x,
                               offset_y + box->bb_y + child->y);
          child = child->next;
        }
      }
      break;

    case MJX_BOX_DIAGONAL:
      mjx_buf_draw_line(buf,
        (int)(offset_x + box->bb_x + box->diag_x1),
        (int)(offset_y + box->bb_y - box->height + box->diag_y1),
        (int)(offset_x + box->bb_x + box->diag_x2),
        (int)(offset_y + box->bb_y - box->height + box->diag_y2),
        box->diag_thickness, r->fg_color);
      break;

    case MJX_BOX_GLUE:
      break;
  }
}

mjx_buf* mjx_render_box(mjx_renderer* r, mjx_box* box) {
  if (!r || !box) return NULL;

  double min_x = 1e9, min_y = 1e9, max_x = -1e9, max_y = -1e9;
  compute_extents(r, box, 0, 0, &min_x, &min_y, &max_x, &max_y);

  if (min_x > max_x) { min_x = 0; max_x = 1; }
  if (min_y > max_y) { min_y = 0; max_y = 1; }

  unsigned int w = (unsigned int)ceil(max_x - min_x) + 2;
  unsigned int h = (unsigned int)ceil(max_y - min_y) + 2;

  if (w < 1) w = 1;
  if (h < 1) h = 1;

  mjx_buf* buf = mjx_buf_alloc(w, h, r->bg_color);
  if (!buf) return NULL;
  buf->fg_color = r->fg_color;

  render_box_internal(r, box, buf, 1 - min_x, 1 - min_y);
  return buf;
}

mjx_renderer* mjx_renderer_create(mjx_font* font) {
  mjx_renderer* r = (mjx_renderer*)calloc(1, sizeof(mjx_renderer));
  if (!r) return NULL;
  r->font = font;
  r->fallback_font = NULL;
  r->fg_color = 0x000000FF;
  r->bg_color = 0x00000000;
  return r;
}

void mjx_renderer_destroy(mjx_renderer* r) {
  free(r);
}
