#include "ast.h"
#include <stdio.h>

typedef void (*mjx_visitor_fn)(mjx_node* node, void* userdata);

void mjx_visit_preorder(mjx_node* node, mjx_visitor_fn fn, void* userdata) {
  if (!node) return;
  fn(node, userdata);
  for (size_t i = 0; i < node->child_count; i++) {
    mjx_visit_preorder(node->children[i], fn, userdata);
  }
}

void mjx_visit_postorder(mjx_node* node, mjx_visitor_fn fn, void* userdata) {
  if (!node) return;
  for (size_t i = 0; i < node->child_count; i++) {
    mjx_visit_postorder(node->children[i], fn, userdata);
  }
  fn(node, userdata);
}

static void dump_node(mjx_node* node, void* data) {
  FILE* f = (FILE*)data;
  fprintf(f, "%p [label=\"%s", (void*)node, mjx_node_type_str(node));
  if (node->text && node->text[0]) {
    fprintf(f, ": %s", node->text);
  }
  fprintf(f, "\"];\n");
  for (size_t i = 0; i < node->child_count; i++) {
    fprintf(f, "%p -> %p;\n", (void*)node, (void*)node->children[i]);
  }
}

void mjx_node_dump_dot(mjx_node* root, FILE* f) {
  fprintf(f, "digraph MathAST {\n");
  mjx_visit_preorder(root, dump_node, f);
  fprintf(f, "}\n");
}
