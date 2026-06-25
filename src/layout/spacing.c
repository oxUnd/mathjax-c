#include "layout.h"
#include <math.h>

/* TeX spacing table (from TeXbook p.170, via MathJax MmlNode.ts) */
/* Indices: [prevClass][curClass] -> value
 *   -1 = no space
 *    0 = 0
 *    1 = thinmathspace (1/6 em)
 *    2 = mediummathspace (2/9 em)
 *    3 = thickmathspace (5/18 em)
 */
static const int TEXSPACE[8][8] = {
  /* ORD  OP   BIN  REL  OPEN CLOSE PUNCT INNER */
  {  0,   1,   2,   3,   0,   0,   0,   1 }, /* ORD */
  {  1,   1,   0,   3,   0,   0,   0,   1 }, /* OP */
  {  2,   2,   0,   0,   2,   0,   0,   2 }, /* BIN */
  {  3,   3,   0,   0,   3,   0,   0,   3 }, /* REL */
  {  0,   0,   0,   0,   0,   0,   0,   0 }, /* OPEN */
  {  0,  -1,   2,   3,   0,   0,   0,   1 }, /* CLOSE */
  {  1,   1,   0,   1,   1,   1,   1,   1 }, /* PUNCT */
  {  1,  -1,   2,   3,   1,   0,   1,   1 }, /* INNER */
};

static const double TEXSPACE_VALUES[] = {
  0.0,
  1.0 / 6.0,  /* thinmathspace */
  2.0 / 9.0,  /* mediummathspace */
  5.0 / 18.0, /* thickmathspace */
};

double mjx_tex_spacing(mjx_texclass prev, mjx_texclass cur, int script_level) {
  if (prev == MJX_TEXCLASS_NONE || cur == MJX_TEXCLASS_NONE) return 0.0;

  int p = (int)prev;
  int c = (int)cur;

  if (p < 0 || p > 7) p = 0;
  if (c < 0 || c > 7) c = 0;

  int space = TEXSPACE[p][c];
  if (space < 0) return -1.0;  /* no space */

  /* No spacing in script styles */
  if (script_level > 0 && space >= 0) {
    /* Only some spaces are suppressed in scripts */
    /* In TeX, all spaces are suppressed in scripts */
    return 0.0;
  }

  return TEXSPACE_VALUES[space];
}

/* Compute mu-based spacing: 1mu = 1/18 em */
double mjx_mu_to_em(double mu, double font_size) {
  return mu * font_size / 18.0;
}

/* Compute a "thin space" in the current font */
double mjx_thin_space(double font_size) {
  return font_size / 6.0;
}
