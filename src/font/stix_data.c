#include "font_data.h"

/*
 * STIX Two Math fallback data.
 *
 * HarfBuzz exposes many MATH-table assemblies, but the bundled STIX font does
 * not report every useful size variant through hb_ot_math_get_glyph_variants().
 * Keep those font-specific fallback glyph sequences in one place so layout
 * code stays generic: ask the MATH table first, then ask this table.
 */

int mjx_stix_get_accent_variants(uint32_t codepoint,
                                 mjx_stix_accent_variants* variants) {
  static const unsigned int hat[] = {691, 1395, 1396, 1397, 1398, 1399};
  static const unsigned int caron[] = {692, 1400, 1401, 1402, 1403, 1404};
  static const unsigned int tilde[] = {693, 1405, 1406, 1407, 1408, 1409};
  static const unsigned int bar[] = {695, 1457, 1458, 1459, 1460, 1461};
  static const unsigned int right_arrow[] = {1277, 1478, 1479, 1480, 1481, 1482};
  static const unsigned int left_arrow[] = {1276, 1472, 1473, 1474, 1475, 1476};
  static const unsigned int leftright_arrow[] = {1283};

  if (!variants) return 0;
  variants->glyphs = 0;
  variants->count = 0;
  variants->min_width_factor = 0.50;
  variants->stretch_tolerance = 1.18;

  switch (codepoint) {
    case 0x0302:
    case 0x02C6:
      variants->glyphs = hat;
      variants->count = sizeof(hat) / sizeof(hat[0]);
      variants->min_width_factor = 0.36;
      return 1;
    case 0x030C:
    case 0x02C7:
      variants->glyphs = caron;
      variants->count = sizeof(caron) / sizeof(caron[0]);
      variants->min_width_factor = 0.36;
      return 1;
    case 0x0303:
    case 0x02DC:
      variants->glyphs = tilde;
      variants->count = sizeof(tilde) / sizeof(tilde[0]);
      return 1;
    case 0x0304:
    case 0x00AF:
    case 0x203E:
      variants->glyphs = bar;
      variants->count = sizeof(bar) / sizeof(bar[0]);
      return 1;
    case 0x20D7:
    case 0x2192:
      variants->glyphs = right_arrow;
      variants->count = sizeof(right_arrow) / sizeof(right_arrow[0]);
      return 1;
    case 0x2190:
      variants->glyphs = left_arrow;
      variants->count = sizeof(left_arrow) / sizeof(left_arrow[0]);
      return 1;
    case 0x2194:
      variants->glyphs = leftright_arrow;
      variants->count = sizeof(leftright_arrow) / sizeof(leftright_arrow[0]);
      return 1;
    default:
      return 0;
  }
}

int mjx_stix_get_delimiter_variants(uint32_t codepoint,
                                    const unsigned int** glyphs,
                                    unsigned int* count) {
  static const unsigned int paren_left[] = {
    1064, 1301, 1302, 1303, 1304, 1305, 1306, 1307, 1308, 1309, 1310, 1311, 1312
  };
  static const unsigned int paren_right[] = {
    1065, 1313, 1314, 1315, 1316, 1317, 1318, 1319, 1320, 1321, 1322, 1323, 1324
  };
  static const unsigned int bracket_left[] = {
    1066, 1325, 1326, 1327, 1328, 1329, 1330, 1331, 1332, 1333, 1334, 1335, 1336
  };
  static const unsigned int bracket_right[] = {
    1067, 1337, 1338, 1339, 1340, 1341, 1342, 1343, 1344, 1345, 1346, 1347, 1348
  };
  static const unsigned int brace_left[] = {
    1068, 1349, 1350, 1351, 1352, 1353, 1354, 1355, 1356, 1357, 1358, 1359, 1360
  };
  static const unsigned int brace_right[] = {
    1069, 1361, 1362, 1363, 1364, 1365, 1366, 1367, 1368, 1369, 1370, 1371, 1372
  };

  if (!glyphs || !count) return 0;
  switch (codepoint) {
    case '(':
      *glyphs = paren_left;
      *count = sizeof(paren_left) / sizeof(paren_left[0]);
      return 1;
    case ')':
      *glyphs = paren_right;
      *count = sizeof(paren_right) / sizeof(paren_right[0]);
      return 1;
    case '[':
      *glyphs = bracket_left;
      *count = sizeof(bracket_left) / sizeof(bracket_left[0]);
      return 1;
    case ']':
      *glyphs = bracket_right;
      *count = sizeof(bracket_right) / sizeof(bracket_right[0]);
      return 1;
    case '{':
      *glyphs = brace_left;
      *count = sizeof(brace_left) / sizeof(brace_left[0]);
      return 1;
    case '}':
      *glyphs = brace_right;
      *count = sizeof(brace_right) / sizeof(brace_right[0]);
      return 1;
    default:
      return 0;
  }
}

int mjx_stix_get_radical_variants(unsigned int base_glyph_id,
                                  const unsigned int** glyphs,
                                  unsigned int* count) {
  static const unsigned int sqrt_variants[] = {1657, 1658, 1659, 1660};
  if (!glyphs || !count || base_glyph_id != sqrt_variants[0]) return 0;
  *glyphs = sqrt_variants;
  *count = sizeof(sqrt_variants) / sizeof(sqrt_variants[0]);
  return 1;
}

int mjx_stix_get_fraction_rule_variants(const unsigned int** glyphs,
                                        unsigned int* count) {
  static const unsigned int overline[] = {1457, 1458, 1459, 1460, 1461};
  if (!glyphs || !count) return 0;
  *glyphs = overline;
  *count = sizeof(overline) / sizeof(overline[0]);
  return 1;
}
