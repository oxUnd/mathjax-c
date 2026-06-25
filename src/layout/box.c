#include "layout.h"
#include "font.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

mjx_box* mjx_box_create(mjx_box_type type) {
  mjx_box* box = (mjx_box*)calloc(1, sizeof(mjx_box));
  if (!box) return NULL;
  box->type = type;
  box->tex_class = MJX_TEXCLASS_NONE;
  return box;
}

mjx_box* mjx_box_create_glyph(mjx_layout_ctx* ctx, uint32_t codepoint) {
  double scale = 1.0;
  if (ctx && ctx->font && ctx->font->em_size > 0) {
    scale = ctx->font_size / ctx->font->em_size;
  }

  mjx_glyph_info info;
  if (!mjx_font_get_glyph(ctx->font, codepoint, &info)) {
    /* Glyph not found, create a small space */
    mjx_box* box = mjx_box_create(MJX_BOX_GLYPH);
    if (!box) return NULL;
    box->codepoint = codepoint;
    box->font_size = ctx->font_size;
    box->width = 0.5 * ctx->font_size;
    box->height = 0.7 * ctx->font_size;
    box->depth = 0.1 * ctx->font_size;
    return box;
  }

  mjx_box* box = mjx_box_create(MJX_BOX_GLYPH);
  if (!box) return NULL;

  box->codepoint = codepoint;
  box->glyph_id = info.glyph_id;
  box->font_size = ctx->font_size;
  box->width = info.advance_width * scale;
  box->height = info.height * scale;
  /* depth = how far below the baseline */
  box->depth = info.depth * scale;
  box->italic_correction = info.italic_correction * scale;

  return box;
}

mjx_box* mjx_box_create_rule(double width, double height, double depth) {
  mjx_box* box = mjx_box_create(MJX_BOX_RULE);
  if (!box) return NULL;
  box->width = width;
  box->height = height;
  box->depth = depth;
  box->rule_thickness = height;
  return box;
}

mjx_box* mjx_box_create_glue(double width, double stretch, double shrink) {
  mjx_box* box = mjx_box_create(MJX_BOX_GLUE);
  if (!box) return NULL;
  box->width = width;
  box->stretch = stretch;
  box->shrink = shrink;
  return box;
}

void mjx_box_add_child(mjx_box* parent, mjx_box* child, double x, double y) {
  if (!parent || !child) return;

  mjx_box_child* c = (mjx_box_child*)calloc(1, sizeof(mjx_box_child));
  if (!c) return;

  c->box = child;
  c->x = x;
  c->y = y;

  /* Add to end of list */
  if (!parent->children) {
    parent->children = c;
  } else {
    mjx_box_child* last = parent->children;
    while (last->next) last = last->next;
    last->next = c;
  }
}

static void destroy_box_children(mjx_box* box) {
  mjx_box_child* c = box->children;
  while (c) {
    mjx_box_child* next = c->next;
    mjx_box_destroy(c->box);
    free(c);
    c = next;
  }
  box->children = NULL;
}

void mjx_box_destroy(mjx_box* box) {
  if (!box) return;
  destroy_box_children(box);
  free(box);
}

/* Create a HBox (horizontal box) with specified children */
mjx_box* mjx_box_create_hbox(mjx_layout_ctx* ctx) {
  (void)ctx;
  return mjx_box_create(MJX_BOX_HBOX);
}

/* Create a VBox (vertical box) with specified children */
mjx_box* mjx_box_create_vbox(mjx_layout_ctx* ctx) {
  (void)ctx;
  return mjx_box_create(MJX_BOX_VBOX);
}

/* Create a TeX Atom box (wrapper with tex class) */
mjx_box* mjx_box_create_atom(mjx_box* inner, mjx_texclass tex_class) {
  mjx_box* atom = mjx_box_create(MJX_BOX_ATOM);
  if (!atom) return NULL;
  atom->tex_class = tex_class;
  if (inner) {
    mjx_box_add_child(atom, inner, 0, 0);
    atom->width = inner->width;
    atom->height = inner->height;
    atom->depth = inner->depth;
  }
  return atom;
}

/* Compute final dimensions of an HBox based on its children */
void mjx_box_hpack(mjx_box* hbox) {
  if (!hbox || hbox->type != MJX_BOX_HBOX) return;

  double width = 0;
  double height = 0;
  double depth = 0;

  mjx_box_child* c = hbox->children;
  while (c) {
    if (c->box) {
      double right = c->x + c->box->width;
      if (right > width) width = right;
      if (c->box->height - c->y > height) {
        height = c->box->height - c->y;
      }
      if (c->y + c->box->depth > depth) {
        depth = c->y + c->box->depth;
      }
    }
    c = c->next;
  }

  hbox->width = width;
  hbox->height = height;
  hbox->depth = depth;
}

/* Compute final dimensions of a VBox based on its children */
void mjx_box_vpack(mjx_box* vbox) {
  if (!vbox || vbox->type != MJX_BOX_VBOX) return;

  double width = 0;
  double height = 0;
  double depth = 0;

  mjx_box_child* c = vbox->children;
  while (c) {
    if (c->box) {
      if (c->box->width > width) width = c->box->width;
      /* For VBox, children are stacked vertically */
      /* y position tracks total accumulated height downward */
    }
    c = c->next;
  }

  vbox->width = width;
  vbox->height = height;
  vbox->depth = depth;
}
