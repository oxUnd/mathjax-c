#include "ast.h"
#include <stdlib.h>
#include <string.h>

/* Node type names */
static const char* NODE_NAMES[] = {
  [MJX_NODE_MROW] = "mrow",
  [MJX_NODE_MI] = "mi",
  [MJX_NODE_MN] = "mn",
  [MJX_NODE_MO] = "mo",
  [MJX_NODE_MTEXT] = "mtext",
  [MJX_NODE_MSPACE] = "mspace",
  [MJX_NODE_MSQRT] = "msqrt",
  [MJX_NODE_MROOT] = "mroot",
  [MJX_NODE_MFRAC] = "mfrac",
  [MJX_NODE_MSUP] = "msup",
  [MJX_NODE_MSUB] = "msub",
  [MJX_NODE_MSUBSUP] = "msubsup",
  [MJX_NODE_MUNDER] = "munder",
  [MJX_NODE_MOVER] = "mover",
  [MJX_NODE_MUNDEROVER] = "munderover",
  [MJX_NODE_MMULTISCRIPTS] = "mmultiscripts",
  [MJX_NODE_MTABLE] = "mtable",
  [MJX_NODE_MTR] = "mtr",
  [MJX_NODE_MTD] = "mtd",
  [MJX_NODE_MLABELEDTR] = "mlabeledtr",
  [MJX_NODE_MPADDED] = "mpadded",
  [MJX_NODE_MSTYLE] = "mstyle",
  [MJX_NODE_MPHANTOM] = "mphantom",
  [MJX_NODE_MENCLOSE] = "menclose",
  [MJX_NODE_MERROR] = "merror",
  [MJX_NODE_MACTION] = "maction",
  [MJX_NODE_MSEMANTICS] = "semantics",
  [MJX_NODE_ANNOTATION] = "annotation",
  [MJX_NODE_ANNOTATION_XML] = "annotation-xml",
  [MJX_NODE_MGLYPH] = "mglyph",
  [MJX_NODE_TEXATOM] = "TeXAtom",
  [MJX_NODE_TEXT] = "text",
  [MJX_NODE_XML] = "XML",
};

mjx_node* mjx_node_create(mjx_node_type type) {
  mjx_node* node = (mjx_node*)calloc(1, sizeof(mjx_node));
  if (!node) return NULL;
  node->type = type;
  node->tex_class = MJX_TEXCLASS_NONE;
  node->prev_class = MJX_TEXCLASS_NONE;
  node->prev_level = 0;
  node->mo_lspace = -1;
  node->mo_rspace = -1;
  node->variant = MJX_VARIANT_NORMAL;
  return node;
}

void mjx_node_destroy(mjx_node* node) {
  if (!node) return;
  /* Free children */
  for (size_t i = 0; i < node->child_count; i++) {
    mjx_node_destroy(node->children[i]);
  }
  free(node->children);
  /* Free text */
  free(node->text);
  /* Free attributes */
  mjx_attr* a = node->attrs;
  while (a) {
    mjx_attr* next = a->next;
    free(a->name);
    free(a->value);
    free(a);
    a = next;
  }
  /* Free properties */
  mjx_prop* p = node->props;
  while (p) {
    mjx_prop* next = p->next;
    free(p->name);
    if (p->free_fn && p->value) p->free_fn(p->value);
    free(p);
    p = next;
  }
  free(node);
}

mjx_node* mjx_node_append(mjx_node* parent, mjx_node* child) {
  if (!parent || !child) return NULL;
  if (parent->child_count >= parent->child_capacity) {
    size_t new_cap = parent->child_capacity ? parent->child_capacity * 2 : 4;
    mjx_node** new_children = (mjx_node**)realloc(parent->children, new_cap * sizeof(mjx_node*));
    if (!new_children) return NULL;
    parent->children = new_children;
    parent->child_capacity = new_cap;
  }
  parent->children[parent->child_count++] = child;
  child->parent = parent;
  return child;
}

mjx_node* mjx_node_remove(mjx_node* parent, mjx_node* child) {
  if (!parent || !child) return NULL;
  size_t j = 0;
  for (size_t i = 0; i < parent->child_count; i++) {
    if (parent->children[i] != child) {
      parent->children[j++] = parent->children[i];
    }
  }
  if (j < parent->child_count) {
    parent->child_count = j;
    child->parent = NULL;
    return child;
  }
  return NULL;
}

mjx_node* mjx_node_insert_at(mjx_node* parent, mjx_node* child, size_t idx) {
  if (!parent || !child) return NULL;
  if (idx > parent->child_count) idx = parent->child_count;
  if (parent->child_count >= parent->child_capacity) {
    size_t new_cap = parent->child_capacity ? parent->child_capacity * 2 : 4;
    mjx_node** new_children = (mjx_node**)realloc(parent->children, new_cap * sizeof(mjx_node*));
    if (!new_children) return NULL;
    parent->children = new_children;
    parent->child_capacity = new_cap;
  }
  memmove(&parent->children[idx + 1], &parent->children[idx],
          (parent->child_count - idx) * sizeof(mjx_node*));
  parent->children[idx] = child;
  parent->child_count++;
  child->parent = parent;
  return child;
}

void mjx_node_set_attr(mjx_node* node, const char* name, const char* value) {
  if (!node || !name) return;
  mjx_attr* a = node->attrs;
  while (a) {
    if (strcmp(a->name, name) == 0) {
      free(a->value);
      a->value = value ? strdup(value) : NULL;
      return;
    }
    a = a->next;
  }
  a = (mjx_attr*)calloc(1, sizeof(mjx_attr));
  if (!a) return;
  a->name = strdup(name);
  a->value = value ? strdup(value) : NULL;
  a->next = node->attrs;
  node->attrs = a;
}

const char* mjx_node_get_attr(const mjx_node* node, const char* name) {
  if (!node || !name) return NULL;
  mjx_attr* a = node->attrs;
  while (a) {
    if (strcmp(a->name, name) == 0) return a->value;
    a = a->next;
  }
  return NULL;
}

void mjx_node_set_prop(mjx_node* node, const char* name, void* value, void (*free_fn)(void*)) {
  if (!node || !name) return;
  mjx_prop* p = node->props;
  while (p) {
    if (strcmp(p->name, name) == 0) {
      if (p->free_fn && p->value) p->free_fn(p->value);
      p->value = value;
      p->free_fn = free_fn;
      return;
    }
    p = p->next;
  }
  p = (mjx_prop*)calloc(1, sizeof(mjx_prop));
  if (!p) return;
  p->name = strdup(name);
  p->value = value;
  p->free_fn = free_fn;
  p->next = node->props;
  node->props = p;
}

void* mjx_node_get_prop(const mjx_node* node, const char* name) {
  if (!node || !name) return NULL;
  mjx_prop* p = node->props;
  while (p) {
    if (strcmp(p->name, name) == 0) return p->value;
    p = p->next;
  }
  return NULL;
}

mjx_node* mjx_node_copy(const mjx_node* node, int keep_ids) {
  if (!node) return NULL;
  mjx_node* copy = mjx_node_create(node->type);
  if (!copy) return NULL;
  copy->flags = node->flags;
  copy->tex_class = node->tex_class;
  copy->mo_form = node->mo_form;
  copy->mo_lspace = node->mo_lspace;
  copy->mo_rspace = node->mo_rspace;
  copy->mo_stretchy = node->mo_stretchy;
  copy->mo_symmetric = node->mo_symmetric;
  copy->mo_fence = node->mo_fence;
  copy->mo_largeop = node->mo_largeop;
  copy->mo_movablelimits = node->mo_movablelimits;
  copy->variant = node->variant;
  if (node->text) {
    copy->text = strdup(node->text);
    copy->text_len = node->text_len;
  }
  mjx_attr* a = node->attrs;
  while (a) {
    if (strcmp(a->name, "id") != 0 || keep_ids) {
      mjx_node_set_attr(copy, a->name, a->value);
    }
    a = a->next;
  }
  for (size_t i = 0; i < node->child_count; i++) {
    mjx_node* child_copy = mjx_node_copy(node->children[i], keep_ids);
    if (child_copy) mjx_node_append(copy, child_copy);
  }
  return copy;
}

const char* mjx_node_type_str(const mjx_node* node) {
  if (!node) return "null";
  if (node->type < sizeof(NODE_NAMES) / sizeof(NODE_NAMES[0]) && NODE_NAMES[node->type]) {
    return NODE_NAMES[node->type];
  }
  return "unknown";
}
