#include "font.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_TABLES_H
#include <hb.h>
#include <hb-ot.h>
#include <hb-ft.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

mjx_font* mjx_font_load(const char* path, double pt_size, unsigned int dpi) {
  mjx_font* font = (mjx_font*)calloc(1, sizeof(mjx_font));
  if (!font) return NULL;

  strncpy(font->path, path, sizeof(font->path) - 1);
  font->pt_size = pt_size;

  FT_Library ft_lib;
  if (FT_Init_FreeType(&ft_lib) != 0) {
    free(font);
    return NULL;
  }

  FT_Face ft_face;
  if (FT_New_Face(ft_lib, path, 0, &ft_face) != 0) {
    FT_Done_FreeType(ft_lib);
    free(font);
    return NULL;
  }

  double scale = pt_size * dpi / 72.0;
  FT_Set_Char_Size(ft_face, 0, (FT_F26Dot6)(pt_size * 64), dpi, dpi);

  hb_face_t* hb_face = hb_ft_face_create_referenced(ft_face);
  if (!hb_face) {
    free(font);
    FT_Done_Face(ft_face);
    FT_Done_FreeType(ft_lib);
    return NULL;
  }
  hb_font_t* hb_f = hb_font_create(hb_face);
  hb_ft_font_set_funcs(hb_f);

  font->ft_library = ft_lib;
  font->ft_face = ft_face;
  font->hb_font = hb_f;
  font->hb_face = hb_face;
  font->scale = scale;
  font->em_size = pt_size * dpi / 72.0;

  /* Get x-height from OS/2 table */
  TT_OS2* os2 = FT_Get_Sfnt_Table(ft_face, FT_SFNT_OS2);
  if (os2 && os2->sxHeight) {
    font->x_height = os2->sxHeight * font->scale / ft_face->units_per_EM;
  } else {
    /* Fallback: approximate as 0.45 em */
    font->x_height = 0.45 * font->em_size;
  }

  return font;
}

void mjx_font_unload(mjx_font* font) {
  if (!font) return;
  if (font->hb_font) hb_font_destroy(font->hb_font);
  if (font->hb_face) hb_face_destroy(font->hb_face);
  if (font->ft_face) FT_Done_Face(font->ft_face);
  if (font->ft_library) FT_Done_FreeType(font->ft_library);
  free(font);
}

int mjx_font_has_glyph(mjx_font* font, uint32_t codepoint) {
  FT_Face face = (FT_Face)font->ft_face;
  FT_UInt glyph_idx = FT_Get_Char_Index(face, codepoint);
  return glyph_idx != 0;
}

int mjx_font_get_glyph(mjx_font* font, uint32_t codepoint, mjx_glyph_info* info) {
  FT_Face face = (FT_Face)font->ft_face;
  FT_UInt glyph_idx = FT_Get_Char_Index(face, codepoint);
  if (glyph_idx == 0) return 0;

  FT_Load_Glyph(face, glyph_idx, FT_LOAD_NO_SCALE);

  info->codepoint = codepoint;
  info->glyph_id = glyph_idx;
  info->advance_width = face->glyph->metrics.horiAdvance * font->scale / face->units_per_EM;
  info->height = face->glyph->metrics.horiBearingY * font->scale / face->units_per_EM;
  info->depth = (face->glyph->metrics.height - face->glyph->metrics.horiBearingY) * font->scale / face->units_per_EM;

  /* Get italic correction from HarfBuzz MATH table */
  info->italic_correction = 0;
  if (hb_ot_math_has_data(font->hb_face)) {
    hb_position_t ic = hb_ot_math_get_glyph_italics_correction(font->hb_font, glyph_idx);
    info->italic_correction = ic * font->scale / face->units_per_EM;

    hb_position_t accent = hb_ot_math_get_glyph_top_accent_attachment(font->hb_font, glyph_idx);
    info->top_accent_attachment = accent * font->scale / face->units_per_EM;
  } else {
    info->top_accent_attachment = info->advance_width / 2;
  }

  return 1;
}

unsigned char* mjx_font_render_glyph(mjx_font* font, uint32_t codepoint,
                                      unsigned int* out_w, unsigned int* out_h,
                                      int* out_left, int* out_top) {
  FT_Face face = (FT_Face)font->ft_face;
  FT_UInt glyph_idx = FT_Get_Char_Index(face, codepoint);
  if (glyph_idx == 0) return NULL;

  if (FT_Load_Glyph(face, glyph_idx, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL) != 0) {
    return NULL;
  }

  FT_Bitmap* bitmap = &face->glyph->bitmap;
  unsigned int w = bitmap->width;
  unsigned int h = bitmap->rows;

  /* Convert grayscale to alpha-only if needed */
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

unsigned char* mjx_font_render_glyph_size(mjx_font* font, uint32_t codepoint,
                                           double font_size,
                                           unsigned int* out_w, unsigned int* out_h,
                                           int* out_left, int* out_top) {
  if (!font || font_size <= 0.0) {
    return mjx_font_render_glyph(font, codepoint, out_w, out_h, out_left, out_top);
  }
  FT_Face face = (FT_Face)font->ft_face;
  FT_Set_Pixel_Sizes(face, 0, (FT_UInt)fmax(1.0, round(font_size)));
  unsigned char* alpha = mjx_font_render_glyph(font, codepoint, out_w, out_h, out_left, out_top);
  FT_Set_Pixel_Sizes(face, 0, (FT_UInt)fmax(1.0, round(font->em_size)));
  return alpha;
}

unsigned char* mjx_font_render_glyph_id(mjx_font* font, unsigned int glyph_id,
                                         unsigned int* out_w, unsigned int* out_h,
                                         int* out_left, int* out_top) {
  FT_Face face = (FT_Face)font->ft_face;
  if (glyph_id == 0) return NULL;

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

unsigned char* mjx_font_render_glyph_id_size(mjx_font* font, unsigned int glyph_id,
                                              double font_size,
                                              unsigned int* out_w, unsigned int* out_h,
                                              int* out_left, int* out_top) {
  if (!font || font_size <= 0.0) {
    return mjx_font_render_glyph_id(font, glyph_id, out_w, out_h, out_left, out_top);
  }
  FT_Face face = (FT_Face)font->ft_face;
  FT_Set_Pixel_Sizes(face, 0, (FT_UInt)fmax(1.0, round(font_size)));
  unsigned char* alpha = mjx_font_render_glyph_id(font, glyph_id, out_w, out_h, out_left, out_top);
  FT_Set_Pixel_Sizes(face, 0, (FT_UInt)fmax(1.0, round(font->em_size)));
  return alpha;
}

double mjx_font_glyph_width(mjx_font* font, unsigned int glyph_id) {
  FT_Face face = (FT_Face)font->ft_face;
  if (glyph_id == 0) return 0;
  if (FT_Load_Glyph(face, glyph_id, FT_LOAD_NO_SCALE) != 0) return 0;
  return face->glyph->metrics.horiAdvance * font->scale / face->units_per_EM;
}

double mjx_font_glyph_height(mjx_font* font, unsigned int glyph_id) {
  FT_Face face = (FT_Face)font->ft_face;
  if (glyph_id == 0) return 0;
  if (FT_Load_Glyph(face, glyph_id, FT_LOAD_NO_SCALE) != 0) return 0;
  return face->glyph->metrics.height * font->scale / face->units_per_EM;
}

int mjx_font_get_glyph_id_info(mjx_font* font, unsigned int glyph_id,
                               mjx_glyph_id_info* info) {
  if (!font || !info || glyph_id == 0) return 0;
  FT_Face face = (FT_Face)font->ft_face;
  if (FT_Load_Glyph(face, glyph_id, FT_LOAD_NO_SCALE) != 0) return 0;

  info->glyph_id = glyph_id;
  info->advance_width = face->glyph->metrics.horiAdvance * font->scale / face->units_per_EM;
  info->height = face->glyph->metrics.horiBearingY * font->scale / face->units_per_EM;
  info->depth = (face->glyph->metrics.height - face->glyph->metrics.horiBearingY) *
                font->scale / face->units_per_EM;
  return 1;
}

int mjx_font_get_variants(mjx_font* font, uint32_t codepoint,
                           mjx_glyph_variant* variants, unsigned int max_count,
                           unsigned int* out_count) {
  if (!hb_ot_math_has_data(font->hb_face)) return 0;

  FT_Face face = (FT_Face)font->ft_face;
  FT_UInt glyph_idx = FT_Get_Char_Index(face, codepoint);
  if (glyph_idx == 0) return 0;

  unsigned int count = 0;
  hb_ot_math_get_glyph_variants(font->hb_font, glyph_idx, HB_DIRECTION_TTB, 0, &count, NULL);

  if (count > 0) {
    hb_ot_math_glyph_variant_t* hb_variants =
      (hb_ot_math_glyph_variant_t*)calloc(count, sizeof(hb_ot_math_glyph_variant_t));
    hb_ot_math_get_glyph_variants(font->hb_font, glyph_idx, HB_DIRECTION_TTB, 0, &count, hb_variants);

    unsigned int n = count < max_count ? count : max_count;
    for (unsigned int i = 0; i < n; i++) {
      variants[i].glyph_id = hb_variants[i].glyph;

      /* Load the variant glyph to get its metrics */
      FT_Load_Glyph(face, hb_variants[i].glyph, FT_LOAD_NO_SCALE);
      variants[i].advance_width = face->glyph->metrics.horiAdvance * font->scale / face->units_per_EM;
      variants[i].height = face->glyph->metrics.height * font->scale / face->units_per_EM;
    }
    *out_count = count;
    free(hb_variants);
    return 1;
  }
  return 0;
}

int mjx_font_get_parts(mjx_font* font, uint32_t codepoint,
                        mjx_glyph_part* parts, unsigned int max_count,
                        unsigned int* out_count, double* italics_correction) {
  if (!hb_ot_math_has_data(font->hb_face)) return 0;

  FT_Face face = (FT_Face)font->ft_face;
  FT_UInt glyph_idx = FT_Get_Char_Index(face, codepoint);
  if (glyph_idx == 0) return 0;

  unsigned int count = 0;
  hb_ot_math_get_glyph_assembly(font->hb_font, glyph_idx, HB_DIRECTION_TTB, 0, &count, NULL, NULL);

  if (count > 0) {
    hb_ot_math_glyph_part_t* hb_parts =
      (hb_ot_math_glyph_part_t*)calloc(count, sizeof(hb_ot_math_glyph_part_t));
    hb_position_t ic;
    hb_ot_math_get_glyph_assembly(font->hb_font, glyph_idx, HB_DIRECTION_TTB, 0, &count, hb_parts, &ic);

    if (italics_correction) {
      *italics_correction = ic * font->scale / face->units_per_EM;
    }

    unsigned int n = count < max_count ? count : max_count;
    for (unsigned int i = 0; i < n; i++) {
      parts[i].glyph_id = hb_parts[i].glyph;
      parts[i].is_extender = (hb_parts[i].flags & HB_OT_MATH_GLYPH_PART_FLAG_EXTENDER) != 0;
      FT_Load_Glyph(face, hb_parts[i].glyph, FT_LOAD_NO_SCALE);
      parts[i].full_advance = face->glyph->metrics.height * font->scale / face->units_per_EM;
    }
    *out_count = count;
    free(hb_parts);
    return 1;
  }
  return 0;
}

int mjx_font_get_horizontal_variants(mjx_font* font, uint32_t codepoint,
                                      mjx_glyph_variant* variants, unsigned int max_count,
                                      unsigned int* out_count) {
  if (!hb_ot_math_has_data(font->hb_face)) return 0;

  FT_Face face = (FT_Face)font->ft_face;
  FT_UInt glyph_idx = FT_Get_Char_Index(face, codepoint);
  if (glyph_idx == 0) return 0;

  unsigned int count = 0;
  hb_ot_math_get_glyph_variants(font->hb_font, glyph_idx, HB_DIRECTION_LTR, 0, &count, NULL);

  if (count > 0) {
    hb_ot_math_glyph_variant_t* hb_variants =
      (hb_ot_math_glyph_variant_t*)calloc(count, sizeof(hb_ot_math_glyph_variant_t));
    hb_ot_math_get_glyph_variants(font->hb_font, glyph_idx, HB_DIRECTION_LTR, 0, &count, hb_variants);

    unsigned int n = count < max_count ? count : max_count;
    for (unsigned int i = 0; i < n; i++) {
      variants[i].glyph_id = hb_variants[i].glyph;
      FT_Load_Glyph(face, hb_variants[i].glyph, FT_LOAD_NO_SCALE);
      variants[i].advance_width = face->glyph->metrics.horiAdvance * font->scale / face->units_per_EM;
      variants[i].height = face->glyph->metrics.height * font->scale / face->units_per_EM;
    }
    *out_count = count;
    free(hb_variants);
    return 1;
  }
  return 0;
}

int mjx_font_get_horizontal_parts(mjx_font* font, uint32_t codepoint,
                                   mjx_glyph_part* parts, unsigned int max_count,
                                   unsigned int* out_count, double* italics_correction) {
  if (!hb_ot_math_has_data(font->hb_face)) return 0;

  FT_Face face = (FT_Face)font->ft_face;
  FT_UInt glyph_idx = FT_Get_Char_Index(face, codepoint);
  if (glyph_idx == 0) return 0;

  unsigned int count = 0;
  hb_ot_math_get_glyph_assembly(font->hb_font, glyph_idx, HB_DIRECTION_LTR, 0, &count, NULL, NULL);

  if (count > 0) {
    hb_ot_math_glyph_part_t* hb_parts =
      (hb_ot_math_glyph_part_t*)calloc(count, sizeof(hb_ot_math_glyph_part_t));
    hb_position_t ic;
    hb_ot_math_get_glyph_assembly(font->hb_font, glyph_idx, HB_DIRECTION_LTR, 0, &count, hb_parts, &ic);

    if (italics_correction) {
      *italics_correction = ic * font->scale / face->units_per_EM;
    }

    unsigned int n = count < max_count ? count : max_count;
    for (unsigned int i = 0; i < n; i++) {
      parts[i].glyph_id = hb_parts[i].glyph;
      parts[i].is_extender = (hb_parts[i].flags & HB_OT_MATH_GLYPH_PART_FLAG_EXTENDER) != 0;
      parts[i].full_advance = hb_parts[i].full_advance * font->scale / face->units_per_EM;
      if (parts[i].full_advance <= 0.0) {
        FT_Load_Glyph(face, hb_parts[i].glyph, FT_LOAD_NO_SCALE);
        parts[i].full_advance = face->glyph->metrics.horiAdvance * font->scale / face->units_per_EM;
      }
    }
    *out_count = count;
    free(hb_parts);
    return 1;
  }
  return 0;
}

double mjx_font_script_scale(mjx_font* font, int script_level) {
  if (script_level <= 0) return 1.0;
  if (script_level == 1) {
    if (hb_ot_math_has_data(font->hb_face)) {
      return hb_ot_math_get_constant(font->hb_font,
               HB_OT_MATH_CONSTANT_SCRIPT_PERCENT_SCALE_DOWN) / 100.0;
    }
    return 0.8;
  }
  /* script_level >= 2 */
  if (hb_ot_math_has_data(font->hb_face)) {
    return hb_ot_math_get_constant(font->hb_font,
             HB_OT_MATH_CONSTANT_SCRIPT_SCRIPT_PERCENT_SCALE_DOWN) / 100.0;
  }
  return 0.6;
}

double mjx_font_get_glyph_kern(mjx_font* font, mjx_glyph_info* glyph,
                                double correction_height) {
  if (!hb_ot_math_has_data(font->hb_face)) return 0;
  FT_Face face = (FT_Face)font->ft_face;

  hb_position_t kern = hb_ot_math_get_glyph_kerning(
    font->hb_font, glyph->glyph_id,
    HB_OT_MATH_KERN_TOP_RIGHT,
    (hb_position_t)(correction_height * face->units_per_EM / font->scale));

  return kern * font->scale / face->units_per_EM;
}
