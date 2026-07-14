#include "layout.h"
#include "font.h"
#include <math.h>

/* 
 * Layout for msup, msub, msubsup nodes.
 * Follows TeX algorithm for script placement.
 */

mjx_box* mjx_layout_scripts(mjx_layout_ctx* ctx, mjx_node* node, int display) {
  mjx_math_constants* mc = &ctx->font->math_constants;

  if (node->child_count < 2) {
    /* No base or scripts, just layout what we have */
    if (node->child_count == 0) return mjx_box_create(MJX_BOX_HBOX);
    return mjx_layout_node(ctx, node->children[0], display);
  }

  mjx_box* base = mjx_layout_node(ctx, node->children[0], display);
  if (!base) return NULL;

  mjx_box* sub = NULL;
  mjx_box* sup = NULL;

  ctx->script_depth++;

  /* Determine which children are subscript and superscript */
  if (node->type == MJX_NODE_MSUP) {
    sup = mjx_layout_node(ctx, node->children[1], 0);
  } else if (node->type == MJX_NODE_MSUB) {
    sub = mjx_layout_node(ctx, node->children[1], 0);
  } else if (node->type == MJX_NODE_MSUBSUP) {
    sub = mjx_layout_node(ctx, node->children[1], 0);
    if (node->child_count > 2) {
      sup = mjx_layout_node(ctx, node->children[2], 0);
    }
  }

  ctx->script_depth--;

  /* Calculate script positions */
  double sub_shift = mc->subscript_shift_down;
  double sup_shift = display ? mc->superscript_shift_up : mc->superscript_shift_up_cramped;

  mjx_box* result = mjx_box_create(MJX_BOX_HBOX);
  if (!result) {
    if (sub) mjx_box_destroy(sub);
    if (sup) mjx_box_destroy(sup);
    return base;
  }
  result->tex_class = base->tex_class;

  double x = 0;

  /* Place the base */
  mjx_box_add_child(result, base, x, 0);

  double base_italic_corr = 0;
  /* Get italic correction from last glyph in base */
  {
    mjx_box* last = base;
    mjx_box_child* c = base->children;
    while (c && c->next) c = c->next;
    if (c && c->box) last = c->box;
    base_italic_corr = last->italic_correction;
  }

  if (sup) {
    double min_sup_shift = base->height - mc->superscript_baseline_drop_max;
    if (sup_shift < min_sup_shift) sup_shift = min_sup_shift;
    {
      double sup_bottom_min = mc->superscript_bottom_min + sup->depth;
      if (sup_shift < sup_bottom_min) sup_shift = sup_bottom_min;
    }
  }

  if (sub) {
    double min_sub_shift = base->depth + mc->subscript_baseline_drop_min;
    if (sub_shift < min_sub_shift) sub_shift = min_sub_shift;
    {
      double sub_top_limit = mc->subscript_top_max;
      double needed = sup ? 0.0 : (sub->height - sub_top_limit);
      if (needed > sub_shift) sub_shift = needed;
    }
  }

  double sup_y = -sup_shift;
  double sub_y = sub_shift;

  if (sup && sub) {
    double gap = sub_y - sub->height - (sup_y + sup->depth);
    if (gap < mc->sub_superscript_gap_min) {
      sub_y += mc->sub_superscript_gap_min - gap;
    }

    {
      double bottom = -(sup_y + sup->depth);
      if (bottom > mc->superscript_bottom_max_with_subscript) {
        sup_y -= bottom - mc->superscript_bottom_max_with_subscript;
      }
    }
  }

  double script_x = x + base->width;
  double sup_x = script_x + base_italic_corr;
  double sub_x = script_x;

  if (sup) {
    mjx_box_add_child(result, sup, sup_x, sup_y);
  }

  if (sub) {
    mjx_box_add_child(result, sub, sub_x, sub_y);
  }

  result->width = base->width;
  if (sup && sup_x + sup->width > result->width) result->width = sup_x + sup->width;
  if (sub && sub_x + sub->width > result->width) result->width = sub_x + sub->width;
  result->width += mc->space_after_script;

  result->height = base->height;
  if (sup) {
    double sup_height = sup->height - sup_y;
    if (sup_height > result->height) result->height = sup_height;
  }

  result->depth = base->depth;
  if (sub) {
    double sub_depth = sub_y + sub->depth;
    if (sub_depth > result->depth) result->depth = sub_depth;
  }

  return result;
}
