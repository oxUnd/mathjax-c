#include "ast.h"
#include <stdlib.h>

static mjx_node* default_create(mjx_factory* f, mjx_node_type type) {
  (void)f;
  return mjx_node_create(type);
}

static void default_destroy(mjx_factory* f, mjx_node* node) {
  (void)f;
  mjx_node_destroy(node);
}

mjx_factory* mjx_factory_create(void) {
  mjx_factory* f = (mjx_factory*)calloc(1, sizeof(mjx_factory));
  if (!f) return NULL;
  f->create = default_create;
  f->destroy = default_destroy;
  return f;
}

void mjx_factory_destroy(mjx_factory* f) {
  free(f);
}

mjx_node* mjx_factory_create_node(mjx_factory* f, mjx_node_type type) {
  if (!f || !f->create) return NULL;
  return f->create(f, type);
}
