#ifndef MJX_FONT_DATA_H
#define MJX_FONT_DATA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const unsigned int* glyphs;
  unsigned int count;
  double min_width_factor;
  double stretch_tolerance;
} mjx_stix_accent_variants;

typedef struct {
  unsigned int glyph_id;
  int is_extender;
  double full_advance_units;
} mjx_stix_glyph_part;

int mjx_stix_get_accent_variants(uint32_t codepoint,
                                 mjx_stix_accent_variants* variants);
int mjx_stix_get_delimiter_variants(uint32_t codepoint,
                                    const unsigned int** glyphs,
                                    unsigned int* count);
int mjx_stix_get_radical_variants(unsigned int base_glyph_id,
                                  const unsigned int** glyphs,
                                  unsigned int* count);
int mjx_stix_get_fraction_rule_variants(const unsigned int** glyphs,
                                        unsigned int* count);
int mjx_stix_get_horizontal_parts(uint32_t codepoint,
                                  const mjx_stix_glyph_part** parts,
                                  unsigned int* count);
int mjx_stix_get_largeop_display_variant(uint32_t codepoint,
                                         unsigned int* glyph_id);

#ifdef __cplusplus
}
#endif

#endif /* MJX_FONT_DATA_H */
