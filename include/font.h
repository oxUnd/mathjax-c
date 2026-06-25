#ifndef MJX_FONT_H
#define MJX_FONT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Font metrics constants from MATH table */
typedef struct {
  double script_percent_scale_down;
  double script_script_percent_scale_down;
  double display_operator_min_height;
  double axis_height;
  double accent_base_height;
  double flattened_accent_base_height;
  double subscript_shift_down;
  double subscript_top_max;
  double subscript_baseline_drop_min;
  double superscript_shift_up;
  double superscript_shift_up_cramped;
  double superscript_bottom_min;
  double superscript_baseline_drop_max;
  double sub_superscript_gap_min;
  double superscript_bottom_max_with_subscript;
  double space_after_script;
  double upper_limit_gap_min;
  double upper_limit_baseline_rise_min;
  double lower_limit_gap_min;
  double lower_limit_baseline_drop_min;
  double stack_top_shift_up;
  double stack_top_display_style_shift_up;
  double stack_bottom_shift_down;
  double stack_bottom_display_style_shift_down;
  double stack_gap_min;
  double stack_display_style_gap_min;
  double stretch_stack_top_shift_up;
  double stretch_stack_bottom_shift_down;
  double stretch_stack_gap_above_min;
  double stretch_stack_gap_below_min;
  double fraction_numerator_shift_up;
  double fraction_numerator_display_style_shift_up;
  double fraction_denominator_shift_down;
  double fraction_denominator_display_style_shift_down;
  double fraction_numerator_gap_min;
  double fraction_numerator_display_style_gap_min;
  double fraction_rule_thickness;
  double fraction_denominator_gap_min;
  double fraction_denominator_display_style_gap_min;
  double skewed_fraction_horizontal_gap;
  double skewed_fraction_vertical_gap;
  double overbar_vertical_gap;
  double overbar_rule_thickness;
  double overbar_extra_ascender;
  double underbar_vertical_gap;
  double underbar_rule_thickness;
  double underbar_extra_descender;
  double radical_vertical_gap;
  double radical_display_style_vertical_gap;
  double radical_rule_thickness;
  double radical_extra_ascender;
  double radical_kern_before_degree;
  double radical_kern_after_degree;
  double radical_degree_bottom_raise_percent;
} mjx_math_constants;

/* Glyph info */
typedef struct {
  uint32_t codepoint;
  unsigned int glyph_id;
  double advance_width;
  double height;
  double depth;
  double italic_correction;
  double top_accent_attachment;
} mjx_glyph_info;

typedef struct {
  unsigned int glyph_id;
  double advance_width;
  double height;
  double depth;
} mjx_glyph_id_info;

/* Per-font data */
typedef struct mjx_font {
  void* ft_library;    /* FT_Library */
  void* ft_face;       /* FT_Face */
  void* hb_font;       /* hb_font_t */
  void* hb_face;       /* hb_face_t */
  double pt_size;      /* point size */
  double scale;        /* scale factor for metrics (points to pixels) */
  double em_size;      /* em size in font units */
  double x_height;     /* x-height in pixels */
  double sup_raise;    /* superscript raise amount */
  double sub_drop;     /* subscript drop amount */
  mjx_math_constants math_constants;
  char path[1024];
} mjx_font;

/* Font loading */
mjx_font* mjx_font_load(const char* path, double pt_size, unsigned int dpi);
void      mjx_font_unload(mjx_font* font);

/* Glyph metrics */
int mjx_font_get_glyph(mjx_font* font, uint32_t codepoint, mjx_glyph_info* info);

/* Rasterize glyph to 8-bit alpha buffer (returns width, height, buffer) */
unsigned char* mjx_font_render_glyph(mjx_font* font, uint32_t codepoint,
                                     unsigned int* out_w, unsigned int* out_h,
                                     int* out_left, int* out_top);
unsigned char* mjx_font_render_glyph_size(mjx_font* font, uint32_t codepoint,
                                          double font_size,
                                          unsigned int* out_w, unsigned int* out_h,
                                          int* out_left, int* out_top);

/* Stretchy glyph variants */
typedef struct {
  unsigned int glyph_id;
  double advance_width;
  double height;
} mjx_glyph_variant;

typedef struct {
  unsigned int glyph_id;
  int is_extender;
  double full_advance;
} mjx_glyph_part;

int mjx_font_get_variants(mjx_font* font, uint32_t codepoint,
                          mjx_glyph_variant* variants, unsigned int max_count,
                          unsigned int* out_count);

int mjx_font_get_parts(mjx_font* font, uint32_t codepoint,
                       mjx_glyph_part* parts, unsigned int max_count,
                       unsigned int* out_count, double* italics_correction);

int mjx_font_get_horizontal_variants(mjx_font* font, uint32_t codepoint,
                                     mjx_glyph_variant* variants, unsigned int max_count,
                                     unsigned int* out_count);

int mjx_font_get_horizontal_parts(mjx_font* font, uint32_t codepoint,
                                  mjx_glyph_part* parts, unsigned int max_count,
                                  unsigned int* out_count, double* italics_correction);

/* Get subscript/superscript scale factor */
double mjx_font_script_scale(mjx_font* font, int script_level);

/* Get glyph kern at correction height */
double mjx_font_get_glyph_kern(mjx_font* font, mjx_glyph_info* glyph,
                                double correction_height);

/* Query support */
int mjx_font_has_glyph(mjx_font* font, uint32_t codepoint);

/* Load math constants into struct */
int mjx_font_load_math_constants(mjx_font* font, mjx_math_constants* mc);

/* Render by glyph_id (for variants/assembly) */
unsigned char* mjx_font_render_glyph_id(mjx_font* font, unsigned int glyph_id,
                                         unsigned int* out_w, unsigned int* out_h,
                                         int* out_left, int* out_top);
unsigned char* mjx_font_render_glyph_id_size(mjx_font* font, unsigned int glyph_id,
                                             double font_size,
                                             unsigned int* out_w, unsigned int* out_h,
                                             int* out_left, int* out_top);

/* Get glyph metrics by glyph_id */
double mjx_font_glyph_width(mjx_font* font, unsigned int glyph_id);
double mjx_font_glyph_height(mjx_font* font, unsigned int glyph_id);
int    mjx_font_get_glyph_id_info(mjx_font* font, unsigned int glyph_id,
                                  mjx_glyph_id_info* info);

#ifdef __cplusplus
}
#endif

#endif /* MJX_FONT_H */
