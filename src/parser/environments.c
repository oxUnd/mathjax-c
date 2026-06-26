#include "parser.h"
#include "ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static mjx_node* make_fence(mjx_node* inner, const char* ldelim, const char* rdelim) {
  mjx_node* row = mjx_node_create(MJX_NODE_MROW);
  if (!row) return inner;
  mjx_node_set_attr(row, "left", ldelim);
  mjx_node_set_attr(row, "right", rdelim);
  mjx_node_append(row, inner);
  return row;
}

static mjx_node* env_matrix(mjx_parser* parser, const char* env_name, mjx_node* content) {
  (void)parser;
  mjx_node_set_attr(content, "align", "c");
  mjx_node_set_attr(content, "col_space", "0.7em");
  mjx_node_set_attr(content, "row_space", "0.25em");
  if (strcmp(env_name, "pmatrix") == 0) {
    mjx_node_set_attr(content, "left", "(");
    mjx_node_set_attr(content, "right", ")");
    return make_fence(content, "(", ")");
  } else if (strcmp(env_name, "bmatrix") == 0) {
    return make_fence(content, "[", "]");
  } else if (strcmp(env_name, "Bmatrix") == 0) {
    return make_fence(content, "{", "}");
  } else if (strcmp(env_name, "vmatrix") == 0) {
    return make_fence(content, "|", "|");
  } else if (strcmp(env_name, "Vmatrix") == 0) {
    return make_fence(content, "\xE2\x80\x96", "\xE2\x80\x96");
  }
  return content;
}

static mjx_node* env_cases(mjx_parser* parser, const char* env_name, mjx_node* content) {
  (void)parser; (void)env_name;
  return make_fence(content, "{", ".");
}

static mjx_node* env_aligned(mjx_parser* parser, const char* env_name, mjx_node* content) {
  (void)parser; (void)env_name;
  mjx_node_set_attr(content, "align", "rcl");
  mjx_node_set_attr(content, "row_space", "0.5em");
  return content;
}

static mjx_node* env_gather(mjx_parser* parser, const char* env_name, mjx_node* content) {
  (void)parser; (void)env_name;
  mjx_node_set_attr(content, "align", "c");
  mjx_node_set_attr(content, "row_space", "0.5em");
  return content;
}

void mjx_parser_init_environments(mjx_parser* parser) {
  if (!parser) return;
  mjx_parser_register_env(parser, "matrix", env_matrix);
  mjx_parser_register_env(parser, "pmatrix", env_matrix);
  mjx_parser_register_env(parser, "bmatrix", env_matrix);
  mjx_parser_register_env(parser, "Bmatrix", env_matrix);
  mjx_parser_register_env(parser, "vmatrix", env_matrix);
  mjx_parser_register_env(parser, "Vmatrix", env_matrix);
  mjx_parser_register_env(parser, "cases", env_cases);
  mjx_parser_register_env(parser, "aligned", env_aligned);
  mjx_parser_register_env(parser, "gathered", env_gather);
  mjx_parser_register_env(parser, "split", env_aligned);
  mjx_parser_register_env(parser, "align", env_aligned);
  mjx_parser_register_env(parser, "alignat", env_aligned);
  mjx_parser_register_env(parser, "flalign", env_aligned);
  mjx_parser_register_env(parser, "multline", env_gather);
}
