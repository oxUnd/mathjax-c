#ifndef MJX_PARSER_H
#define MJX_PARSER_H

#include "ast.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Parser context */
typedef struct mjx_parser mjx_parser;

/* Macro definition */
typedef struct {
  char* name;
  int arg_count;       /* number of arguments (0..9) */
  char* replacement;   /* replacement string with #1..#N */
  int is_primitive;    /* built-in command, not user-defined */
} mjx_macro;

/* Environment handler */
typedef mjx_node* (*mjx_env_handler)(mjx_parser* p, const char* env_name, mjx_node* content);

/* Operator dictionary entry */
typedef struct {
  const char* symbol;
  int form;           /* 0=prefix, 1=infix, 2=postfix */
  int lspace;
  int rspace;
  int stretchy;
  int symmetric;
  int fence;
  int largeop;
  int movablelimits;
  int accent;
  int texclass;
} mjx_operator;

/* Parser configuration */
typedef struct {
  mjx_factory* factory;
  mjx_macro** macros;
  size_t macro_count;
  mjx_operator* operators;
  size_t operator_count;
  mjx_env_handler* env_handlers;
  const char** env_names;
  size_t env_count;
  int display_math;    /* nonzero = displaystyle mode */
} mjx_parse_opts;

/* Create/destroy parser */
mjx_parser* mjx_parser_create(mjx_parse_opts* opts);
void        mjx_parser_destroy(mjx_parser* parser);

/* Parse LaTeX math string into AST */
mjx_node* mjx_parser_parse_latex(mjx_parser* parser, const char* latex, int display);

/* Parse MathML XML string into AST */
mjx_node* mjx_parser_parse_mathml(mjx_parser* parser, const char* xml, int display);

/* Register a macro */
int mjx_parser_register_macro(mjx_parser* parser, const char* name,
                               int arg_count, const char* replacement);

/* Register an environment handler */
int mjx_parser_register_env(mjx_parser* parser, const char* name,
                             mjx_env_handler handler);

/* Lookup operator data */
const mjx_operator* mjx_parser_lookup_operator(mjx_parser* parser, const char* symbol);

/* Get last error */
const char* mjx_parser_error(mjx_parser* parser);

/* Macro create/destroy */
mjx_macro* mjx_macro_create(const char* name, int arg_count, const char* replacement);
void       mjx_macro_destroy(mjx_macro* m);

/* Symbol lookup functions */
int mjx_lookup_greek(const char* cmd, uint32_t* cp, int* tex_class);
int mjx_lookup_binop(const char* cmd, uint32_t* cp, int* tex_class);
int mjx_lookup_relation(const char* cmd, uint32_t* cp, int* tex_class);
int mjx_lookup_largeop(const char* cmd, uint32_t* cp, int* tex_class, int* is_operator);
int mjx_lookup_misc(const char* cmd, uint32_t* cp, int* tex_class);
int mjx_lookup_accent(const char* cmd, uint32_t* cp);
const char* mjx_lookup_delimiter(const char* name);

/* Initialize environment handlers */
void mjx_parser_init_environments(mjx_parser* parser);

/* Default operator dictionary */
mjx_operator* mjx_default_operators(size_t* count);

/* Default macros (both primitives and predefined) */
mjx_macro** mjx_default_macros(size_t* count);

#ifdef __cplusplus
}
#endif

#endif /* MJX_PARSER_H */
