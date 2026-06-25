#include "font.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ot.h>
#include <hb-ft.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* 
 * Render a glyph by glyph_id (not codepoint) to an alpha buffer.
 * Used for stretchy variants and assembly parts.
 */
unsigned char* mjx_font_render_glyph_id(mjx_font* font, unsigned int glyph_id,
                                         unsigned int* out_w, unsigned int* out_h,
                                         int* out_left, int* out_top) {
  FT_Face face = (FT_Face)font->ft_face;
  if (FT_Load_Glyph(face, glyph_id, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL) != 0) {
    return NULL;
  }
  FT_Bitmap* bitmap = &face->glyph->bitmap;
  unsigned int w = bitmap->width;
  unsigned int h = bitmap->rows;

  unsigned char* alpha = (unsigned char*)malloc(w * h);
  if (!alpha) return NULL;

  if (bitmap->pixel_mode == FT_PIXEL_MODE_GRAY) {
    memcpy(alpha, bitmap->buffer, w * h);
  } else if (bitmap->pixel_mode == FT_PIXEL_MODE_MONO) {
    for (unsigned int y = 0; y < h; y++) {
      for (unsigned int x = 0; x < w; x++) {
        int byte_idx = y * bitmap->pitch + x / 8;
        int bit_idx = 7 - (x % 8);
        alpha[y * w + x] = (bitmap->buffer[byte_idx] >> bit_idx) & 1 ? 255 : 0;
      }
    }
  } else {
    memset(alpha, 255, w * h);
  }

  *out_w = w;
  *out_h = h;
  *out_left = face->glyph->bitmap_left;
  *out_top = face->glyph->bitmap_top;
  return alpha;
}

/* Get glyph width from glyph_id */
double mjx_font_glyph_width(mjx_font* font, unsigned int glyph_id) {
  FT_Face face = (FT_Face)font->ft_face;
  FT_Load_Glyph(face, glyph_id, FT_LOAD_NO_SCALE);
  return face->glyph->metrics.horiAdvance * font->scale / face->units_per_EM;
}

/* Get glyph height from glyph_id */
double mjx_font_glyph_height(mjx_font* font, unsigned int glyph_id) {
  FT_Face face = (FT_Face)font->ft_face;
  FT_Load_Glyph(face, glyph_id, FT_LOAD_NO_SCALE);
  return face->glyph->metrics.height * font->scale / face->units_per_EM;
}
