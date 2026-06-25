#include "font.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ot.h>
#include <math.h>
#include <string.h>

#define GET_CONSTANT(font, constant) \
  (hb_ot_math_get_constant(font->hb_font, constant) * \
   font->scale / ((FT_Face)font->ft_face)->units_per_EM) / 64.0 * 64.0

#define GET_CONSTANT_VAL(font, constant, def) \
  (hb_ot_math_has_data(font->hb_face) ? \
   (hb_ot_math_get_constant(font->hb_font, constant) * \
    font->scale / ((FT_Face)font->ft_face)->units_per_EM) : def)

static double get_val(mjx_font* font, hb_ot_math_constant_t c) {
  hb_position_t val = hb_ot_math_get_constant(font->hb_font, c);
  FT_Face face = (FT_Face)font->ft_face;
  return val * font->scale / face->units_per_EM;
}

/* Populate all math constants from the MATH table */
int mjx_font_load_math_constants(mjx_font* font, mjx_math_constants* mc) {
  if (!font || !mc) return 0;
  if (!hb_ot_math_has_data(font->hb_face)) return 0;

  memset(mc, 0, sizeof(*mc));

  mc->script_percent_scale_down =
    hb_ot_math_get_constant(font->hb_font, HB_OT_MATH_CONSTANT_SCRIPT_PERCENT_SCALE_DOWN) / 100.0;
  mc->script_script_percent_scale_down =
    hb_ot_math_get_constant(font->hb_font, HB_OT_MATH_CONSTANT_SCRIPT_SCRIPT_PERCENT_SCALE_DOWN) / 100.0;

  mc->display_operator_min_height = get_val(font, HB_OT_MATH_CONSTANT_DISPLAY_OPERATOR_MIN_HEIGHT);
  mc->axis_height = get_val(font, HB_OT_MATH_CONSTANT_AXIS_HEIGHT);
  mc->accent_base_height = get_val(font, HB_OT_MATH_CONSTANT_ACCENT_BASE_HEIGHT);
  mc->flattened_accent_base_height = get_val(font, HB_OT_MATH_CONSTANT_FLATTENED_ACCENT_BASE_HEIGHT);
  mc->subscript_shift_down = get_val(font, HB_OT_MATH_CONSTANT_SUBSCRIPT_SHIFT_DOWN);
  mc->subscript_top_max = get_val(font, HB_OT_MATH_CONSTANT_SUBSCRIPT_TOP_MAX);
  mc->subscript_baseline_drop_min = get_val(font, HB_OT_MATH_CONSTANT_SUBSCRIPT_BASELINE_DROP_MIN);
  mc->superscript_shift_up = get_val(font, HB_OT_MATH_CONSTANT_SUPERSCRIPT_SHIFT_UP);
  mc->superscript_shift_up_cramped = get_val(font, HB_OT_MATH_CONSTANT_SUPERSCRIPT_SHIFT_UP_CRAMPED);
  mc->superscript_bottom_min = get_val(font, HB_OT_MATH_CONSTANT_SUPERSCRIPT_BOTTOM_MIN);
  mc->superscript_baseline_drop_max = get_val(font, HB_OT_MATH_CONSTANT_SUPERSCRIPT_BASELINE_DROP_MAX);
  mc->sub_superscript_gap_min = get_val(font, HB_OT_MATH_CONSTANT_SUB_SUPERSCRIPT_GAP_MIN);
  mc->superscript_bottom_max_with_subscript = get_val(font, HB_OT_MATH_CONSTANT_SUPERSCRIPT_BOTTOM_MAX_WITH_SUBSCRIPT);
  mc->space_after_script = get_val(font, HB_OT_MATH_CONSTANT_SPACE_AFTER_SCRIPT);
  mc->upper_limit_gap_min = get_val(font, HB_OT_MATH_CONSTANT_UPPER_LIMIT_GAP_MIN);
  mc->upper_limit_baseline_rise_min = get_val(font, HB_OT_MATH_CONSTANT_UPPER_LIMIT_BASELINE_RISE_MIN);
  mc->lower_limit_gap_min = get_val(font, HB_OT_MATH_CONSTANT_LOWER_LIMIT_GAP_MIN);
  mc->lower_limit_baseline_drop_min = get_val(font, HB_OT_MATH_CONSTANT_LOWER_LIMIT_BASELINE_DROP_MIN);
  mc->stack_top_shift_up = get_val(font, HB_OT_MATH_CONSTANT_STACK_TOP_SHIFT_UP);
  mc->stack_top_display_style_shift_up = get_val(font, HB_OT_MATH_CONSTANT_STACK_TOP_DISPLAY_STYLE_SHIFT_UP);
  mc->stack_bottom_shift_down = get_val(font, HB_OT_MATH_CONSTANT_STACK_BOTTOM_SHIFT_DOWN);
  mc->stack_bottom_display_style_shift_down = get_val(font, HB_OT_MATH_CONSTANT_STACK_BOTTOM_DISPLAY_STYLE_SHIFT_DOWN);
  mc->stack_gap_min = get_val(font, HB_OT_MATH_CONSTANT_STACK_GAP_MIN);
  mc->stack_display_style_gap_min = get_val(font, HB_OT_MATH_CONSTANT_STACK_DISPLAY_STYLE_GAP_MIN);
  mc->stretch_stack_top_shift_up = get_val(font, HB_OT_MATH_CONSTANT_STRETCH_STACK_TOP_SHIFT_UP);
  mc->stretch_stack_bottom_shift_down = get_val(font, HB_OT_MATH_CONSTANT_STRETCH_STACK_BOTTOM_SHIFT_DOWN);
  mc->stretch_stack_gap_above_min = get_val(font, HB_OT_MATH_CONSTANT_STRETCH_STACK_GAP_ABOVE_MIN);
  mc->stretch_stack_gap_below_min = get_val(font, HB_OT_MATH_CONSTANT_STRETCH_STACK_GAP_BELOW_MIN);
  mc->fraction_numerator_shift_up = get_val(font, HB_OT_MATH_CONSTANT_FRACTION_NUMERATOR_SHIFT_UP);
  mc->fraction_numerator_display_style_shift_up = get_val(font, HB_OT_MATH_CONSTANT_FRACTION_NUMERATOR_DISPLAY_STYLE_SHIFT_UP);
  mc->fraction_denominator_shift_down = get_val(font, HB_OT_MATH_CONSTANT_FRACTION_DENOMINATOR_SHIFT_DOWN);
  mc->fraction_denominator_display_style_shift_down = get_val(font, HB_OT_MATH_CONSTANT_FRACTION_DENOMINATOR_DISPLAY_STYLE_SHIFT_DOWN);
  mc->fraction_numerator_gap_min = get_val(font, HB_OT_MATH_CONSTANT_FRACTION_NUMERATOR_GAP_MIN);
  mc->fraction_numerator_display_style_gap_min = get_val(font, HB_OT_MATH_CONSTANT_FRACTION_NUM_DISPLAY_STYLE_GAP_MIN);
  mc->fraction_rule_thickness = get_val(font, HB_OT_MATH_CONSTANT_FRACTION_RULE_THICKNESS);
  mc->fraction_denominator_gap_min = get_val(font, HB_OT_MATH_CONSTANT_FRACTION_DENOMINATOR_GAP_MIN);
  mc->fraction_denominator_display_style_gap_min = get_val(font, HB_OT_MATH_CONSTANT_FRACTION_DENOM_DISPLAY_STYLE_GAP_MIN);
  mc->skewed_fraction_horizontal_gap = get_val(font, HB_OT_MATH_CONSTANT_SKEWED_FRACTION_HORIZONTAL_GAP);
  mc->skewed_fraction_vertical_gap = get_val(font, HB_OT_MATH_CONSTANT_SKEWED_FRACTION_VERTICAL_GAP);
  mc->overbar_vertical_gap = get_val(font, HB_OT_MATH_CONSTANT_OVERBAR_VERTICAL_GAP);
  mc->overbar_rule_thickness = get_val(font, HB_OT_MATH_CONSTANT_OVERBAR_RULE_THICKNESS);
  mc->overbar_extra_ascender = get_val(font, HB_OT_MATH_CONSTANT_OVERBAR_EXTRA_ASCENDER);
  mc->underbar_vertical_gap = get_val(font, HB_OT_MATH_CONSTANT_UNDERBAR_VERTICAL_GAP);
  mc->underbar_rule_thickness = get_val(font, HB_OT_MATH_CONSTANT_UNDERBAR_RULE_THICKNESS);
  mc->underbar_extra_descender = get_val(font, HB_OT_MATH_CONSTANT_UNDERBAR_EXTRA_DESCENDER);
  mc->radical_vertical_gap = get_val(font, HB_OT_MATH_CONSTANT_RADICAL_VERTICAL_GAP);
  mc->radical_display_style_vertical_gap = get_val(font, HB_OT_MATH_CONSTANT_RADICAL_DISPLAY_STYLE_VERTICAL_GAP);
  mc->radical_rule_thickness = get_val(font, HB_OT_MATH_CONSTANT_RADICAL_RULE_THICKNESS);
  mc->radical_extra_ascender = get_val(font, HB_OT_MATH_CONSTANT_RADICAL_EXTRA_ASCENDER);
  mc->radical_kern_before_degree = get_val(font, HB_OT_MATH_CONSTANT_RADICAL_KERN_BEFORE_DEGREE);
  mc->radical_kern_after_degree = get_val(font, HB_OT_MATH_CONSTANT_RADICAL_KERN_AFTER_DEGREE);
  mc->radical_degree_bottom_raise_percent =
    hb_ot_math_get_constant(font->hb_font, HB_OT_MATH_CONSTANT_RADICAL_DEGREE_BOTTOM_RAISE_PERCENT) / 100.0;

  font->math_constants = *mc;
  return 1;
}
