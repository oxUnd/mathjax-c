#ifndef MJX_AST_H
#define MJX_AST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* MathML node types */
typedef enum {
  MJX_NODE_MROW,
  MJX_NODE_MI,
  MJX_NODE_MN,
  MJX_NODE_MO,
  MJX_NODE_MTEXT,
  MJX_NODE_MSPACE,
  MJX_NODE_MSQRT,
  MJX_NODE_MROOT,
  MJX_NODE_MFRAC,
  MJX_NODE_MSUP,
  MJX_NODE_MSUB,
  MJX_NODE_MSUBSUP,
  MJX_NODE_MUNDER,
  MJX_NODE_MOVER,
  MJX_NODE_MUNDEROVER,
  MJX_NODE_MMULTISCRIPTS,
  MJX_NODE_MTABLE,
  MJX_NODE_MTR,
  MJX_NODE_MTD,
  MJX_NODE_MLABELEDTR,
  MJX_NODE_MPADDED,
  MJX_NODE_MSTYLE,
  MJX_NODE_MPHANTOM,
  MJX_NODE_MENCLOSE,
  MJX_NODE_MERROR,
  MJX_NODE_MACTION,
  MJX_NODE_MSEMANTICS,
  MJX_NODE_ANNOTATION,
  MJX_NODE_ANNOTATION_XML,
  MJX_NODE_MGLYPH,
  MJX_NODE_TEXATOM,
  MJX_NODE_TEXT,        /* text leaf node */
  MJX_NODE_XML,          /* raw XML node */
} mjx_node_type;

/* Node flags */
#define MJX_FLAG_IS_INFERRED (1u << 0)
#define MJX_FLAG_IS_SPACELIKE (1u << 1)
#define MJX_FLAG_IS_EMBELLISHED (1u << 2)
#define MJX_FLAG_LINEBREAK_CONTAINER (1u << 3)

/* Math variant */
typedef enum {
  MJX_VARIANT_NORMAL,
  MJX_VARIANT_BOLD,
  MJX_VARIANT_ITALIC,
  MJX_VARIANT_BOLD_ITALIC,
  MJX_VARIANT_DOUBLE_STRUCK,
  MJX_VARIANT_FRAKTUR,
  MJX_VARIANT_BOLD_FRAKTUR,
  MJX_VARIANT_SCRIPT,
  MJX_VARIANT_BOLD_SCRIPT,
  MJX_VARIANT_SANS_SERIF,
  MJX_VARIANT_BOLD_SANS_SERIF,
  MJX_VARIANT_SANS_SERIF_ITALIC,
  MJX_VARIANT_SANS_SERIF_BOLD_ITALIC,
  MJX_VARIANT_MONOSPACE,
  MJX_VARIANT_INITIAL,
  MJX_VARIANT_TAILED,
  MJX_VARIANT_LOOPED,
  MJX_VARIANT_STRETCHED,
} mjx_variant;

/* TeX class for spacing */
typedef enum {
  MJX_TEXCLASS_ORD = 0,
  MJX_TEXCLASS_OP  = 1,
  MJX_TEXCLASS_BIN = 2,
  MJX_TEXCLASS_REL = 3,
  MJX_TEXCLASS_OPEN = 4,
  MJX_TEXCLASS_CLOSE = 5,
  MJX_TEXCLASS_PUNCT = 6,
  MJX_TEXCLASS_INNER = 7,
  MJX_TEXCLASS_NONE = -1,
} mjx_texclass;

/* MathML attributes attached to a node */
typedef struct mjx_attr {
  char* name;
  char* value;
  struct mjx_attr* next;
} mjx_attr;

/* Node properties (key-value) */
typedef struct mjx_prop {
  char* name;
  void* value;
  void (*free_fn)(void*);
  struct mjx_prop* next;
} mjx_prop;

/* AST node structure */
typedef struct mjx_node {
  mjx_node_type type;
  uint32_t flags;
  size_t child_count;
  size_t child_capacity;
  struct mjx_node** children;
  struct mjx_node* parent;
  mjx_attr* attrs;
  mjx_prop* props;
  mjx_texclass tex_class;
  mjx_texclass prev_class;
  int prev_level;

  /* For text nodes (MI, MN, MO, MTEXT, TEXT) */
  char* text;
  size_t text_len;

  /* For MO operator dictionary data */
  int mo_form;        /* 0=prefix, 1=infix, 2=postfix */
  int mo_lspace;      /* negative means use default */
  int mo_rspace;
  int mo_stretchy;    /* boolean */
  int mo_symmetric;
  int mo_fence;
  int mo_largeop;
  int mo_movablelimits;

  /* Variant for token nodes */
  mjx_variant variant;

  /* Style context (computed) */
  int display_style;
  int script_level;
  int prime_style;

  /* Internal factory reference */
  void* factory_ref;
} mjx_node;

/* Node factory */
typedef struct mjx_factory {
  mjx_node* (*create)(struct mjx_factory* f, mjx_node_type type);
  void (*destroy)(struct mjx_factory* f, mjx_node* node);
  void* userdata;
} mjx_factory;

/* Node type name string */
const char* mjx_node_type_str(const mjx_node* node);

/* Create/destroy nodes */
mjx_node* mjx_node_create(mjx_node_type type);
void      mjx_node_destroy(mjx_node* node);

/* Node tree operations */
mjx_node* mjx_node_append(mjx_node* parent, mjx_node* child);
mjx_node* mjx_node_remove(mjx_node* parent, mjx_node* child);
mjx_node* mjx_node_insert_at(mjx_node* parent, mjx_node* child, size_t idx);

/* Attribute operations */
void mjx_node_set_attr(mjx_node* node, const char* name, const char* value);
const char* mjx_node_get_attr(const mjx_node* node, const char* name);

/* Property operations */
void mjx_node_set_prop(mjx_node* node, const char* name, void* value, void (*free_fn)(void*));
void* mjx_node_get_prop(const mjx_node* node, const char* name);

/* Copy/clone */
mjx_node* mjx_node_copy(const mjx_node* node, int keep_ids);

/* Factory */
mjx_factory* mjx_factory_create(void);
void mjx_factory_destroy(mjx_factory* f);
mjx_node* mjx_factory_create_node(mjx_factory* f, mjx_node_type type);

#ifdef __cplusplus
}
#endif

#endif /* MJX_AST_H */
