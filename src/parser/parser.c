#include "parser.h"
#include "ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

struct mjx_parser {
  mjx_parse_opts opts;
  mjx_node* result;
  char error_msg[512];
  int has_error;
};

typedef struct {
  const char* input;
  const char* pos;
  int display;
  int style_level;
  int limits_mode; /* -1=nolimits, 1=limits, 0=default */
  struct mjx_parser* parser_ref;
} parse_state;

static mjx_node* parse_expression(parse_state* state, int stop_at_brace);

/* Unwrap single-child MROW — if node is MROW with exactly 1 child and no
 * delimiter attributes, detach and return child (caller must destroy node) */
static mjx_node* unwrap_mrow(mjx_node* node) {
  if (!node || node->type != MJX_NODE_MROW || node->child_count != 1)
    return node;
  if (node->attrs != NULL)
    return node;
  mjx_node* child = node->children[0];
  mjx_node_remove(node, child);
  child->parent = node->parent;
  return child;
}
static mjx_node* parse_group(parse_state* state);
static mjx_node* handle_command(parse_state* state, const char* cmd);
static mjx_node* handle_scripts(parse_state* state, mjx_node* base);
static mjx_node* make_operator_node(parse_state* state, uint32_t codepoint,
                                     mjx_texclass tex_class, int is_largeop);
static mjx_node* make_identifier(parse_state* state, const char* text);
static mjx_node* make_number(parse_state* state, const char* text);
static mjx_node* make_operator_str(parse_state* state, const char* text);

static void encode_utf8(uint32_t cp, char out[8]) {
  if (cp < 0x80) {
    out[0] = (char)cp; out[1] = '\0';
  } else if (cp < 0x800) {
    out[0] = (char)(0xC0 | (cp >> 6));
    out[1] = (char)(0x80 | (cp & 0x3F));
    out[2] = '\0';
  } else if (cp < 0x10000) {
    out[0] = (char)(0xE0 | (cp >> 12));
    out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[2] = (char)(0x80 | (cp & 0x3F));
    out[3] = '\0';
  } else {
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    out[4] = '\0';
  }
}

static int is_operator_char(unsigned char c) {
  return c == '+' || c == '-' || c == '*' || c == '/' ||
         c == '=' || c == '<' || c == '>' ||
         c == '(' || c == ')' || c == '[' || c == ']' ||
         c == ',' || c == ';' || c == ':' || c == '!' ||
         c == '|';
}

static uint32_t decode_first_codepoint(const char* text, size_t len) {
  if (!text || len == 0) return 0;
  uint32_t cp = (unsigned char)text[0];
  if ((cp & 0x80) == 0) return cp;
  if ((cp & 0xE0) == 0xC0 && len >= 2) {
    return ((cp & 0x1F) << 6) | ((unsigned char)text[1] & 0x3F);
  }
  if ((cp & 0xF0) == 0xE0 && len >= 3) {
    return ((cp & 0x0F) << 12) |
           (((unsigned char)text[1] & 0x3F) << 6) |
           ((unsigned char)text[2] & 0x3F);
  }
  if ((cp & 0xF8) == 0xF0 && len >= 4) {
    return ((cp & 0x07) << 18) |
           (((unsigned char)text[1] & 0x3F) << 12) |
           (((unsigned char)text[2] & 0x3F) << 6) |
           ((unsigned char)text[3] & 0x3F);
  }
  return cp;
}

static int largeop_default_limits(mjx_node* node) {
  if (!node || !node->text) return 0;
  uint32_t cp = decode_first_codepoint(node->text, node->text_len);
  if (cp >= 0x222B && cp <= 0x2233) return 0; /* integrals use side limits by default */
  return 1;
}

static void skip_spaces(parse_state* state) {
  while (*state->pos && (*state->pos == ' ' || *state->pos == '\t' ||
         *state->pos == '\n' || *state->pos == '\r')) {
    state->pos++;
  }
}

static char* read_cs(parse_state* state) {
  if (*state->pos != '\\') return NULL;
  state->pos++;

  char buf[256];
  size_t i = 0;

  if (isalpha((unsigned char)*state->pos)) {
    while (isalpha((unsigned char)*state->pos) && i < 255) {
      buf[i++] = *state->pos++;
    }
    buf[i] = '\0';
    if (*state->pos == ' ') state->pos++;
  } else if (*state->pos) {
    buf[i++] = *state->pos++;
    buf[i] = '\0';
  } else {
    return strdup("\\");
  }

  return strdup(buf);
}

static char* read_argument(parse_state* state) {
  skip_spaces(state);
  if (!*state->pos) return NULL;

  if (*state->pos == '{') {
    state->pos++;
    int depth = 1;
    size_t cap = 128, len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;

    while (*state->pos && depth > 0) {
      if (*state->pos == '{') depth++;
      else if (*state->pos == '}') {
        depth--;
        if (depth == 0) { state->pos++; break; }
      } else if (*state->pos == '\\') {
        if (len + 2 > cap) { cap *= 2; buf = (char*)realloc(buf, cap); }
        buf[len++] = *state->pos++;
        if (isalpha((unsigned char)*state->pos)) {
          while (isalpha((unsigned char)*state->pos)) {
            if (len + 1 > cap) { cap *= 2; buf = (char*)realloc(buf, cap); }
            buf[len++] = *state->pos++;
          }
        } else if (*state->pos) {
          if (len + 1 > cap) { cap *= 2; buf = (char*)realloc(buf, cap); }
          buf[len++] = *state->pos++;
        }
        continue;
      }
      if (len + 1 > cap) { cap *= 2; buf = (char*)realloc(buf, cap); }
      buf[len++] = *state->pos++;
    }
    buf[len] = '\0';
    return buf;
  }

  if (*state->pos == '\\') {
    return read_cs(state);
  }

  char* buf = (char*)malloc(8);
  if (!buf) return NULL;
  unsigned char c = (unsigned char)*state->pos;
  int len = 1;
  if ((c & 0xE0) == 0xC0) len = 2;
  else if ((c & 0xF0) == 0xE0) len = 3;
  else if ((c & 0xF8) == 0xF0) len = 4;
  memcpy(buf, state->pos, len);
  buf[len] = '\0';
  state->pos += len;
  return buf;
}

static char* read_optional_arg(parse_state* state) {
  skip_spaces(state);
  if (*state->pos == '[') {
    state->pos++;
    char buf[256];
    size_t i = 0;
    int depth = 1;
    while (*state->pos && depth > 0) {
      if (*state->pos == '[') depth++;
      else if (*state->pos == ']') { depth--; if (depth == 0) break; }
      if (i < 255) buf[i++] = *state->pos;
      state->pos++;
    }
    buf[i] = '\0';
    if (*state->pos == ']') state->pos++;
    return strdup(buf);
  }
  return NULL;
}

static mjx_node* make_operator_node(parse_state* state, uint32_t codepoint,
                                     mjx_texclass tex_class, int is_largeop) {
  (void)state;
  mjx_node* node = mjx_node_create(MJX_NODE_MO);
  if (!node) return NULL;

  char utf8[8];
  if (codepoint < 0x80) {
    utf8[0] = codepoint; utf8[1] = '\0';
  } else if (codepoint < 0x800) {
    utf8[0] = 0xC0 | (codepoint >> 6);
    utf8[1] = 0x80 | (codepoint & 0x3F);
    utf8[2] = '\0';
  } else if (codepoint < 0x10000) {
    utf8[0] = 0xE0 | (codepoint >> 12);
    utf8[1] = 0x80 | ((codepoint >> 6) & 0x3F);
    utf8[2] = 0x80 | (codepoint & 0x3F);
    utf8[3] = '\0';
  } else {
    utf8[0] = 0xF0 | (codepoint >> 18);
    utf8[1] = 0x80 | ((codepoint >> 12) & 0x3F);
    utf8[2] = 0x80 | ((codepoint >> 6) & 0x3F);
    utf8[3] = 0x80 | (codepoint & 0x3F);
    utf8[4] = '\0';
  }

  node->text = strdup(utf8);
  node->text_len = strlen(utf8);
  node->tex_class = tex_class;
  node->mo_largeop = is_largeop;
  return node;
}

static mjx_node* make_identifier(parse_state* state, const char* text) {
  (void)state;
  mjx_node* node = mjx_node_create(MJX_NODE_MI);
  if (!node) return NULL;
  node->text = strdup(text);
  node->text_len = strlen(text);
  return node;
}

static mjx_node* make_number(parse_state* state, const char* text) {
  (void)state;
  mjx_node* node = mjx_node_create(MJX_NODE_MN);
  if (!node) return NULL;
  node->text = strdup(text);
  node->text_len = strlen(text);
  return node;
}

static mjx_node* make_operator_str(parse_state* state, const char* text) {
  (void)state;
  mjx_node* node = mjx_node_create(MJX_NODE_MO);
  if (!node) return NULL;
  node->text = strdup(text);
  node->text_len = strlen(text);

  if (text[1] != '\0') { node->tex_class = MJX_TEXCLASS_ORD; return node; }
  unsigned char c = (unsigned char)text[0];
  if (c == '(' || c == '[' || c == '{') node->tex_class = MJX_TEXCLASS_OPEN;
  else if (c == ')' || c == ']' || c == '}') node->tex_class = MJX_TEXCLASS_CLOSE;
  else if (c == '=' || c == '<' || c == '>' || c == ':') node->tex_class = MJX_TEXCLASS_REL;
  else if (c == '+' || c == '-') node->tex_class = MJX_TEXCLASS_BIN;
  else if (c == ',' || c == ';') node->tex_class = MJX_TEXCLASS_PUNCT;
  else node->tex_class = MJX_TEXCLASS_ORD;
  return node;
}

static mjx_node* handle_left_right(parse_state* state, const char* cmd) {
  skip_spaces(state);
  char delim[64];
  size_t di = 0;

  if (*state->pos == '\\') {
    state->pos++;
    while (isalpha((unsigned char)*state->pos) && di < 60) {
      delim[di++] = *state->pos++;
    }
    if (di == 0 && *state->pos && di < 60) {
      delim[di++] = *state->pos++;
    }
    delim[di] = '\0';
    if (*state->pos == ' ') state->pos++;
  } else if (*state->pos) {
    delim[di++] = *state->pos++;
    delim[di] = '\0';
  } else {
    strcpy(delim, ".");
  }

  if (strcmp(cmd, "left") == 0) {
    mjx_node* content = parse_expression(state, 0);
    if (!content) content = mjx_node_create(MJX_NODE_MROW);

    skip_spaces(state);
    if (*state->pos == '\\') {
      state->pos++;
      char right_delim[64];
      size_t ri = 0;
      if (strncmp(state->pos, "right", 5) == 0) {
        state->pos += 5;
        skip_spaces(state);
        if (*state->pos == '\\') {
          state->pos++;
          if (isalpha((unsigned char)*state->pos)) {
            while (isalpha((unsigned char)*state->pos) && ri < 60) {
              right_delim[ri++] = *state->pos++;
            }
          } else if (*state->pos) {
            right_delim[ri++] = *state->pos++;
          }
        } else if (*state->pos) {
          right_delim[ri++] = *state->pos++;
        }
        right_delim[ri] = '\0';
      } else if (isalpha((unsigned char)*state->pos)) {
        while (isalpha((unsigned char)*state->pos) && ri < 60) {
          right_delim[ri++] = *state->pos++;
        }
        right_delim[ri] = '\0';
        if (*state->pos == ' ') state->pos++;
      } else if (*state->pos) {
        right_delim[ri++] = *state->pos++;
        right_delim[ri] = '\0';
      } else {
        strcpy(right_delim, ".");
      }

      mjx_node* row = mjx_node_create(MJX_NODE_MROW);
      if (row && content) {
        mjx_node_set_attr(row, "left", delim);
        mjx_node_set_attr(row, "right", right_delim);
        mjx_node_append(row, content);
      }
      return row;
    }
    return content;
  }
  return NULL;
}

static mjx_node* handle_frac(parse_state* state) {
  char* num = read_argument(state);
  char* den = read_argument(state);
  if (!num || !den) {
    free(num); free(den);
    return NULL;
  }

  mjx_node* frac = mjx_node_create(MJX_NODE_MFRAC);

  {
    parse_state num_state = *state;
    num_state.input = num;
    num_state.pos = num;
    mjx_node* num_node = parse_expression(&num_state, 0);
    if (!num_node) num_node = mjx_node_create(MJX_NODE_MROW);
    mjx_node_append(frac, num_node);
  }

  {
    parse_state den_state = *state;
    den_state.input = den;
    den_state.pos = den;
    mjx_node* den_node = parse_expression(&den_state, 0);
    if (!den_node) den_node = mjx_node_create(MJX_NODE_MROW);
    mjx_node_append(frac, den_node);
  }

  free(num);
  free(den);
  return frac;
}

static mjx_node* handle_sqrt(parse_state* state, const char* cmd) {
  mjx_node* sqrt_node;
  if (strcmp(cmd, "root") == 0) {
    sqrt_node = mjx_node_create(MJX_NODE_MROOT);
    char* degree = read_argument(state);
    skip_spaces(state);
    if (*state->pos == '\\') {
      const char* save = state->pos;
      char* maybe_of = read_cs(state);
      if (!maybe_of || strcmp(maybe_of, "of") != 0) state->pos = save;
      free(maybe_of);
    }
    char* radicand = read_argument(state);
    if (radicand) {
      parse_state rs = *state;
      rs.input = radicand; rs.pos = radicand;
      mjx_node* rn = parse_expression(&rs, 0);
      if (rn) mjx_node_append(sqrt_node, rn);
      free(radicand);
    }
    if (degree) {
      parse_state ds = *state;
      ds.input = degree; ds.pos = degree;
      mjx_node* dn = parse_expression(&ds, 0);
      if (dn) mjx_node_append(sqrt_node, dn);
      free(degree);
    }
  } else {
    sqrt_node = mjx_node_create(MJX_NODE_MSQRT);
    char* degree_opt = read_optional_arg(state);
    char* radicand = read_argument(state);
    if (degree_opt) {
      sqrt_node->type = MJX_NODE_MROOT;
    }
    if (radicand) {
      parse_state rs = *state;
      rs.input = radicand; rs.pos = radicand;
      mjx_node* rn = parse_expression(&rs, 0);
      if (rn) mjx_node_append(sqrt_node, rn);
      free(radicand);
    }
    if (degree_opt) {
      parse_state ds = *state;
      ds.input = degree_opt; ds.pos = degree_opt;
      mjx_node* dn = parse_expression(&ds, 0);
      if (dn) mjx_node_append(sqrt_node, dn);
      free(degree_opt);
    }
  }
  return sqrt_node;
}

static mjx_node* read_script_arg(parse_state* state) {
  skip_spaces(state);
  if (*state->pos == '{') {
    char* arg = read_argument(state);
    if (!arg) return NULL;
    parse_state ss = *state;
    ss.input = arg; ss.pos = arg;
    mjx_node* result = parse_expression(&ss, 0);
    free(arg);
    return result;
  }

  if (*state->pos == '\\') {
    char* cs = read_cs(state);
    if (!cs) return NULL;
    mjx_node* result = handle_command(state, cs);
    free(cs);
    if (result) return result;
    return read_script_arg(state);
  }

  {
    char* text = read_argument(state);
    if (!text) return NULL;

    mjx_node* node = NULL;
    if (text[0] && text[1] == '\0') {
      unsigned char c = (unsigned char)text[0];
      if (isdigit(c) || c == '.') node = make_number(state, text);
      else if (is_operator_char(c)) node = make_operator_str(state, text);
      else node = make_identifier(state, text);
    } else {
      node = make_identifier(state, text);
    }

    free(text);
    return node;
  }
}

static mjx_node* handle_scripts(parse_state* state, mjx_node* base) {
  skip_spaces(state);

  while (*state->pos == '\\') {
    const char* saved = state->pos;
    char* cs = read_cs(state);
    if (!cs) break;
    if (strcmp(cs, "limits") == 0) {
      state->limits_mode = 1;
      free(cs);
      skip_spaces(state);
      continue;
    }
    if (strcmp(cs, "nolimits") == 0) {
      state->limits_mode = -1;
      free(cs);
      skip_spaces(state);
      continue;
    }
    free(cs);
    state->pos = saved;
    break;
  }

  int limits_mode = state->limits_mode;
  state->limits_mode = 0;

  int has_sub = 0, has_sup = 0;
  mjx_node* sub_node = NULL;
  mjx_node* sup_node = NULL;

  if (*state->pos == '^') {
    state->pos++;
    has_sup = 1;
    sup_node = read_script_arg(state);
  }

  skip_spaces(state);

  if (*state->pos == '_') {
    state->pos++;
    has_sub = 1;
    sub_node = read_script_arg(state);
  }

  if (!has_sup && has_sub) {
    skip_spaces(state);
    if (*state->pos == '^') {
      state->pos++;
      has_sup = 1;
      sup_node = read_script_arg(state);
    }
  }

  if ((has_sup || has_sub) && base && base->type == MJX_NODE_MO &&
      limits_mode != -1 &&
      (limits_mode == 1 ||
       ((base->mo_movablelimits ||
         (base->mo_largeop && largeop_default_limits(base))) &&
        state->display && state->style_level == 0))) {
    if (has_sup && has_sub) {
      mjx_node* underover = mjx_node_create(MJX_NODE_MUNDEROVER);
      if (underover) {
        mjx_node_append(underover, base);
        mjx_node_append(underover, sub_node);
        mjx_node_append(underover, sup_node);
      }
      return underover;
    } else if (has_sup) {
      mjx_node* mover = mjx_node_create(MJX_NODE_MOVER);
      if (mover) {
        mjx_node_append(mover, base);
        mjx_node_append(mover, sup_node);
      }
      return mover;
    } else if (has_sub) {
      mjx_node* munder = mjx_node_create(MJX_NODE_MUNDER);
      if (munder) {
        mjx_node_append(munder, base);
        mjx_node_append(munder, sub_node);
      }
      return munder;
    }
  }

  if (has_sup && has_sub) {
    mjx_node* msubsup = mjx_node_create(MJX_NODE_MSUBSUP);
    if (msubsup) {
      if (base) mjx_node_append(msubsup, base);
      if (sub_node) mjx_node_append(msubsup, sub_node);
      if (sup_node) mjx_node_append(msubsup, sup_node);
    }
    return msubsup;
  } else if (has_sup) {
    mjx_node* msup = mjx_node_create(MJX_NODE_MSUP);
    if (msup) {
      if (base) mjx_node_append(msup, base);
      if (sup_node) mjx_node_append(msup, sup_node);
    }
    return msup;
  } else if (has_sub) {
    mjx_node* msub = mjx_node_create(MJX_NODE_MSUB);
    if (msub) {
      if (base) mjx_node_append(msub, base);
      if (sub_node) mjx_node_append(msub, sub_node);
    }
    return msub;
  }

  return base;
}

static mjx_node* handle_operatorname(parse_state* state) {
  char* arg = read_argument(state);
  if (!arg) return NULL;
  mjx_node* op = mjx_node_create(MJX_NODE_MO);
  if (op) {
    op->text = strdup(arg);
    op->text_len = strlen(arg);
    op->tex_class = MJX_TEXCLASS_OP;
    op->mo_movablelimits = 1;
  }
  free(arg);
  return op;
}

static const char* largeop_text_name(const char* cmd) {
  if (strcmp(cmd, "limsup") == 0 || strcmp(cmd, "varlimsup") == 0) return "lim sup";
  if (strcmp(cmd, "liminf") == 0 || strcmp(cmd, "varliminf") == 0) return "lim inf";
  if (strcmp(cmd, "varinjlim") == 0 || strcmp(cmd, "injlim") == 0) return "inj lim";
  if (strcmp(cmd, "varprojlim") == 0 || strcmp(cmd, "projlim") == 0) return "proj lim";
  return cmd;
}

static int largeop_has_movable_limits(const char* cmd) {
  return strcmp(cmd, "lim") == 0 || strcmp(cmd, "limsup") == 0 ||
         strcmp(cmd, "liminf") == 0 || strcmp(cmd, "varlimsup") == 0 ||
         strcmp(cmd, "varliminf") == 0 || strcmp(cmd, "varinjlim") == 0 ||
         strcmp(cmd, "varprojlim") == 0 || strcmp(cmd, "max") == 0 ||
         strcmp(cmd, "min") == 0 || strcmp(cmd, "sup") == 0 ||
         strcmp(cmd, "inf") == 0 || strcmp(cmd, "det") == 0 ||
         strcmp(cmd, "gcd") == 0 || strcmp(cmd, "Pr") == 0 ||
         strcmp(cmd, "injlim") == 0 || strcmp(cmd, "projlim") == 0;
}

static mjx_node* handle_braceop(parse_state* state, int is_underbrace) {
  char* arg = read_argument(state);
  if (!arg) return NULL;

  parse_state s = *state; s.input = arg; s.pos = arg;
  mjx_node* expr = parse_expression(&s, 0);
  free(arg);
  if (!expr) return NULL;

  skip_spaces(state);
  mjx_node* text = NULL;
  uint32_t brace_cp = is_underbrace ? 0x23DF : 0x23DE;

  if (is_underbrace && *state->pos == '_') {
    state->pos++;
    text = read_script_arg(state);
  } else if (!is_underbrace && *state->pos == '^') {
    state->pos++;
    text = read_script_arg(state);
  }

  mjx_node* brace = mjx_node_create(MJX_NODE_MO);
  if (!brace) {
    mjx_node_destroy(expr);
    if (text) mjx_node_destroy(text);
    return NULL;
  }
  char utf8[8];
  if (brace_cp < 0x80) { utf8[0] = (char)brace_cp; utf8[1] = '\0'; }
  else if (brace_cp < 0x800) { utf8[0] = 0xC0 | (brace_cp >> 6); utf8[1] = 0x80 | (brace_cp & 0x3F); utf8[2] = '\0'; }
  else { utf8[0] = 0xE0 | (brace_cp >> 12); utf8[1] = 0x80 | ((brace_cp >> 6) & 0x3F); utf8[2] = 0x80 | (brace_cp & 0x3F); utf8[3] = '\0'; }
  brace->text = strdup(utf8);
  brace->text_len = strlen(utf8);

  if (is_underbrace) {
    mjx_node* result = mjx_node_create(MJX_NODE_MUNDER);
    if (result) {
      mjx_node_append(result, expr);
      if (text) {
        mjx_node* inner = mjx_node_create(MJX_NODE_MUNDER);
        if (inner) {
          mjx_node_append(inner, brace);
          mjx_node_append(inner, text);
          mjx_node_append(result, inner);
        }
      } else {
        mjx_node_append(result, brace);
      }
    }
    return result;
  } else {
    mjx_node* result = mjx_node_create(MJX_NODE_MOVER);
    if (result) {
      mjx_node_append(result, expr);
      if (text) {
        mjx_node* inner = mjx_node_create(MJX_NODE_MOVER);
        if (inner) {
          mjx_node_append(inner, brace);
          mjx_node_append(inner, text);
          mjx_node_append(result, inner);
        }
      } else {
        mjx_node_append(result, brace);
      }
    }
    return result;
  }
}

static mjx_node* handle_overset_underset(parse_state* state, int is_overset) {
  char* arg1 = read_argument(state);
  char* arg2 = read_argument(state);
  if (!arg1 || !arg2) { free(arg1); free(arg2); return NULL; }

  mjx_node* node_type = mjx_node_create(MJX_NODE_MOVER);
  if (is_overset) node_type = mjx_node_create(MJX_NODE_MOVER);
  else node_type = mjx_node_create(MJX_NODE_MUNDER);

  if (node_type) {
    parse_state s1 = *state; s1.input = arg2; s1.pos = arg2;
    mjx_node* base = parse_expression(&s1, 0);
    parse_state s2 = *state; s2.input = arg1; s2.pos = arg1;
    mjx_node* script = parse_expression(&s2, 0);
    if (base) mjx_node_append(node_type, base);
    if (script) mjx_node_append(node_type, script);
  }

  free(arg1); free(arg2);
  return node_type;
}

static mjx_node* parse_argument_node(parse_state* state) {
  char* arg = read_argument(state);
  if (!arg) return NULL;
  parse_state s = *state;
  s.input = arg;
  s.pos = arg;
  mjx_node* content = parse_expression(&s, 0);
  free(arg);
  return content ? content : mjx_node_create(MJX_NODE_MROW);
}

static mjx_node* handle_text_like(parse_state* state) {
  char* arg = read_argument(state);
  if (!arg) return NULL;
  mjx_node* node = mjx_node_create(MJX_NODE_MTEXT);
  if (node) {
    node->text = strdup(arg);
    node->text_len = strlen(arg);
  }
  free(arg);
  return node;
}

static mjx_node* make_mo_codepoint(uint32_t cp, mjx_texclass tex_class) {
  char utf8[8];
  encode_utf8(cp, utf8);
  mjx_node* node = mjx_node_create(MJX_NODE_MO);
  if (node) {
    node->text = strdup(utf8);
    node->text_len = strlen(utf8);
    node->tex_class = tex_class;
  }
  return node;
}

static mjx_node* handle_overunder_accent(parse_state* state, uint32_t cp, int is_under) {
  mjx_node* content = parse_argument_node(state);
  if (!content) return NULL;
  mjx_node* result = mjx_node_create(is_under ? MJX_NODE_MUNDER : MJX_NODE_MOVER);
  if (!result) {
    mjx_node_destroy(content);
    return NULL;
  }
  mjx_node_append(result, content);
  mjx_node* accent = make_mo_codepoint(cp, MJX_TEXCLASS_ORD);
  if (accent) mjx_node_append(result, accent);
  return result;
}

static mjx_node* handle_mathchoice(parse_state* state) {
  mjx_node* first = parse_argument_node(state);
  for (int i = 0; i < 3; i++) {
    mjx_node* ignored = parse_argument_node(state);
    if (ignored) mjx_node_destroy(ignored);
  }
  return first ? first : mjx_node_create(MJX_NODE_MROW);
}

static mjx_node* handle_raisebox(parse_state* state) {
  char* amount = read_argument(state);
  free(amount);
  return parse_argument_node(state);
}

static mjx_node* handle_rule_cmd(parse_state* state) {
  char* width = read_argument(state);
  char* height = read_argument(state);
  mjx_node* space = mjx_node_create(MJX_NODE_MSPACE);
  if (space) {
    mjx_node_set_attr(space, "width", width ? width : "1em");
    mjx_node_set_attr(space, "height", height ? height : "1em");
  }
  free(width);
  free(height);
  return space;
}

static mjx_node* handle_middle(parse_state* state) {
  skip_spaces(state);
  char* delim = read_argument(state);
  if (!delim) return NULL;
  const char* unicode = mjx_lookup_delimiter(delim);
  mjx_node* node = make_operator_str(state, unicode ? unicode : delim);
  if (node) {
    node->tex_class = MJX_TEXCLASS_REL;
    node->mo_fence = 1;
    node->mo_stretchy = 1;
  }
  free(delim);
  return node;
}

static mjx_node* handle_buildrel(parse_state* state) {
  mjx_node* top = parse_argument_node(state);
  skip_spaces(state);
  if (*state->pos == '\\') {
    const char* save = state->pos;
    char* cs = read_cs(state);
    if (!cs || strcmp(cs, "over") != 0) state->pos = save;
    free(cs);
  }

  mjx_node* base = parse_argument_node(state);
  if (!base) base = make_operator_node(state, 0x2192, MJX_TEXCLASS_REL, 0);

  mjx_node* mover = mjx_node_create(MJX_NODE_MOVER);
  if (!mover) {
    if (base) mjx_node_destroy(base);
    if (top) mjx_node_destroy(top);
    return NULL;
  }
  mjx_node_append(mover, base);
  if (top) mjx_node_append(mover, top);
  return mover;
}

static mjx_node* handle_prescript(parse_state* state) {
  mjx_node* pre_sup = parse_argument_node(state);
  mjx_node* pre_sub = parse_argument_node(state);
  mjx_node* base = parse_argument_node(state);

  mjx_node* empty = mjx_node_create(MJX_NODE_MSPACE);
  if (empty) mjx_node_set_attr(empty, "width", "0em");

  mjx_node* script = mjx_node_create(MJX_NODE_MSUBSUP);
  if (script) {
    if (empty) mjx_node_append(script, empty);
    if (pre_sub) mjx_node_append(script, pre_sub);
    if (pre_sup) mjx_node_append(script, pre_sup);
  }

  mjx_node* row = mjx_node_create(MJX_NODE_MROW);
  if (row) {
    if (script) mjx_node_append(row, script);
    if (base) mjx_node_append(row, base);
  }
  return row;
}

static mjx_node* handle_sideset(parse_state* state) {
  mjx_node* left = parse_argument_node(state);
  mjx_node* right = parse_argument_node(state);
  mjx_node* base = parse_argument_node(state);
  if (!base) base = make_operator_node(state, 0x220F, MJX_TEXCLASS_OP, 1);

  mjx_node* row = mjx_node_create(MJX_NODE_MROW);
  if (!row) {
    if (left) mjx_node_destroy(left);
    if (right) mjx_node_destroy(right);
    if (base) mjx_node_destroy(base);
    return NULL;
  }
  if (left) mjx_node_append(row, left);
  if (base) mjx_node_append(row, base);
  if (right) mjx_node_append(row, right);
  return row;
}

static mjx_node* handle_cancelto(parse_state* state) {
  mjx_node* target = parse_argument_node(state);
  mjx_node* content = parse_argument_node(state);
  mjx_node* node = mjx_node_create(MJX_NODE_MENCLOSE);
  if (node) {
    if (content) mjx_node_append(node, content);
    if (target) mjx_node_destroy(target);
    mjx_node_set_attr(node, "notation", "updiagonalstrike");
  }
  return node;
}

static mjx_node* handle_colorbox(parse_state* state, int has_frame) {
  char* frame = NULL;
  if (has_frame) frame = read_argument(state);
  char* color = read_argument(state);
  free(frame);
  free(color);
  mjx_node* content = parse_argument_node(state);
  if (!has_frame) return content;

  mjx_node* node = mjx_node_create(MJX_NODE_MENCLOSE);
  if (node) {
    if (content) mjx_node_append(node, content);
    mjx_node_set_attr(node, "notation", "box");
  } else if (content) {
    return content;
  }
  return node;
}

static mjx_node* handle_definecolor(parse_state* state) {
  char* name = read_argument(state);
  char* model = read_argument(state);
  char* spec = read_argument(state);
  free(name);
  free(model);
  free(spec);
  return NULL;
}

static mjx_node* handle_displaylines(parse_state* state) {
  char* arg = read_argument(state);
  if (!arg) return NULL;
  mjx_node* table = mjx_node_create(MJX_NODE_MTABLE);
  if (table) {
    char* p = arg;
    while (*p) {
      char* row_start = p;
      while (*p && !(*p == '\\' && *(p + 1) == '\\')) p++;
      size_t len = (size_t)(p - row_start);
      char* row_text = (char*)malloc(len + 1);
      if (row_text) {
        memcpy(row_text, row_start, len);
        row_text[len] = '\0';
        parse_state rs = *state; rs.input = row_text; rs.pos = row_text;
        mjx_node* expr = parse_expression(&rs, 0);
        mjx_node* tr = mjx_node_create(MJX_NODE_MTR);
        mjx_node* td = mjx_node_create(MJX_NODE_MTD);
        if (tr && td) {
          if (expr) mjx_node_append(td, expr);
          mjx_node_append(tr, td);
          mjx_node_append(table, tr);
        }
        free(row_text);
      }
      if (*p == '\\' && *(p + 1) == '\\') p += 2;
    }
  }
  free(arg);
  return table;
}

static mjx_node* handle_ref(parse_state* state) {
  char* label = read_argument(state);
  mjx_node* node = mjx_node_create(MJX_NODE_MTEXT);
  if (node) {
    node->text = strdup(label ? label : "");
    node->text_len = strlen(node->text);
  }
  free(label);
  return node;
}

static mjx_node* handle_strut(parse_state* state) {
  (void)state;
  mjx_node* space = mjx_node_create(MJX_NODE_MSPACE);
  if (space) {
    mjx_node_set_attr(space, "width", "0em");
    mjx_node_set_attr(space, "height", "0.7em");
    mjx_node_set_attr(space, "depth", "0.3em");
  }
  return space;
}

static mjx_node* handle_iddots(parse_state* state) {
  mjx_node* row = mjx_node_create(MJX_NODE_MROW);
  if (row) {
    mjx_node_append(row, make_operator_node(state, 0x22C5, MJX_TEXCLASS_ORD, 0));
    mjx_node_append(row, make_operator_node(state, 0x22C5, MJX_TEXCLASS_ORD, 0));
    mjx_node_append(row, make_operator_node(state, 0x22C5, MJX_TEXCLASS_ORD, 0));
  }
  return row;
}

static mjx_node* handle_dot_accent_text(parse_state* state, const char* dots) {
  mjx_node* content = parse_argument_node(state);
  mjx_node* mover = mjx_node_create(MJX_NODE_MOVER);
  mjx_node* text = mjx_node_create(MJX_NODE_MTEXT);
  if (text) {
    text->text = strdup(dots);
    text->text_len = strlen(dots);
  }
  if (mover) {
    if (content) mjx_node_append(mover, content);
    if (text) mjx_node_append(mover, text);
  }
  return mover;
}

static mjx_node* handle_genfrac(parse_state* state) {
  char* left = read_argument(state);
  char* right = read_argument(state);
  char* thick = read_argument(state);
  char* style = read_argument(state);
  char* num = read_argument(state);
  char* den = read_argument(state);
  if (!num || !den) { free(left); free(right); free(thick); free(style); free(num); free(den); return NULL; }

  mjx_node* frac = mjx_node_create(MJX_NODE_MFRAC);
  parse_state ns = *state; ns.input = num; ns.pos = num;
  mjx_node* nn = parse_expression(&ns, 0);
  if (!nn) nn = mjx_node_create(MJX_NODE_MROW);
  mjx_node_append(frac, nn);

  parse_state ds = *state; ds.input = den; ds.pos = den;
  mjx_node* dn = parse_expression(&ds, 0);
  if (!dn) dn = mjx_node_create(MJX_NODE_MROW);
  mjx_node_append(frac, dn);

  if (thick && thick[0]) {
    mjx_node_set_attr(frac, "linethickness", thick);
  }
  if (style && style[0]) {
    mjx_node_set_attr(frac, "displaystyle", style);
  }

  if (left && left[0] && right && right[0]) {
    mjx_node* row = mjx_node_create(MJX_NODE_MROW);
    mjx_node_set_attr(row, "left", left);
    mjx_node_set_attr(row, "right", right);
    mjx_node_set_attr(row, "fixed_delims", "true");
    mjx_node_append(row, frac);
    free(left); free(right); free(thick); free(style); free(num); free(den);
    return row;
  }

  free(left); free(right); free(thick); free(style); free(num); free(den);
  return frac;
}

static mjx_node* handle_overline_underline(parse_state* state, int is_overline) {
  char* arg = read_argument(state);
  if (!arg) return NULL;
  mjx_node* node = mjx_node_create(is_overline ? MJX_NODE_MOVER : MJX_NODE_MUNDER);
  if (node) {
    parse_state s = *state; s.input = arg; s.pos = arg;
    mjx_node* content = parse_expression(&s, 0);
    if (content) mjx_node_append(node, content);
    mjx_node* rule = mjx_node_create(MJX_NODE_MO);
    if (rule) {
      rule->text = strdup("\xE2\x80\xBE");
      rule->text_len = 3;
      rule->tex_class = MJX_TEXCLASS_ORD;
      mjx_node_append(node, rule);
    }
  }
  free(arg);
  return node;
}

static mjx_node* handle_boxed(parse_state* state) {
  mjx_node* content = parse_argument_node(state);
  mjx_node* node = mjx_node_create(MJX_NODE_MENCLOSE);
  if (node) {
    if (content) mjx_node_append(node, content);
    mjx_node_set_attr(node, "notation", "box");
  } else if (content) {
    return content;
  }
  return node;
}

static mjx_node* handle_bbox(parse_state* state) {
  char* opts = read_optional_arg(state);
  mjx_node* content = parse_argument_node(state);
  int has_frame = opts &&
    (strstr(opts, "border") || strstr(opts, "solid") ||
     strstr(opts, "box") || strstr(opts, "frame"));
  free(opts);
  if (!has_frame) return content;

  mjx_node* node = mjx_node_create(MJX_NODE_MENCLOSE);
  if (node) {
    if (content) mjx_node_append(node, content);
    mjx_node_set_attr(node, "notation", "box");
  } else if (content) {
    return content;
  }
  return node;
}

static mjx_node* handle_color(parse_state* state) {
  char* color = read_argument(state);
  char* arg = read_argument(state);
  if (!color || !arg) { free(color); free(arg); return NULL; }
  parse_state s = *state; s.input = arg; s.pos = arg;
  mjx_node* content = parse_expression(&s, 0);
  if (content) mjx_node_set_attr(content, "color", color);
  free(color); free(arg);
  return content;
}

static mjx_node* handle_hspace(parse_state* state) {
  char* arg = read_argument(state);
  mjx_node* space = mjx_node_create(MJX_NODE_MSPACE);
  if (space && arg) {
    char attr[64];
    snprintf(attr, sizeof(attr), "%s", arg);
    mjx_node_set_attr(space, "width", attr);
  }
  free(arg);
  return space;
}

static mjx_node* make_space_node(const char* width) {
  mjx_node* space = mjx_node_create(MJX_NODE_MSPACE);
  if (space && width) {
    mjx_node_set_attr(space, "width", width);
  }
  return space;
}

static mjx_node* handle_phantom(parse_state* state) {
  char* arg = read_argument(state);
  mjx_node* ph = mjx_node_create(MJX_NODE_MPHANTOM);
  if (ph && arg) {
    parse_state s = *state; s.input = arg; s.pos = arg;
    mjx_node* content = parse_expression(&s, 0);
    if (content) mjx_node_append(ph, content);
  }
  free(arg);
  return ph;
}

static mjx_node* handle_smash(parse_state* state) {
  char* arg = read_argument(state);
  mjx_node* node = mjx_node_create(MJX_NODE_MROW);
  if (node && arg) {
    parse_state s = *state; s.input = arg; s.pos = arg;
    mjx_node* content = parse_expression(&s, 0);
    if (content) mjx_node_append(node, content);
  }
  free(arg);
  return node;
}

static mjx_node* handle_extensible_arrow(parse_state* state, uint32_t codepoint) {
  char* opt = read_optional_arg(state);
  char* arg = read_argument(state);

  mjx_node* arrow = mjx_node_create(MJX_NODE_MO);
  if (arrow) {
    char utf8[8];
    encode_utf8(codepoint, utf8);
    arrow->text = strdup(utf8);
    arrow->text_len = strlen(utf8);
    arrow->tex_class = MJX_TEXCLASS_REL;
    arrow->mo_stretchy = 1;
    mjx_node_set_attr(arrow, "stretchy", "true");
  }

  mjx_node* over = NULL;
  if (arg) {
    parse_state s = *state; s.input = arg; s.pos = arg;
    over = parse_expression(&s, 0);
  }

  mjx_node* under = NULL;
  if (opt) {
    parse_state os = *state; os.input = opt; os.pos = opt;
    under = parse_expression(&os, 0);
  }

  mjx_node* node = mjx_node_create(under ? MJX_NODE_MUNDEROVER : MJX_NODE_MOVER);
  if (node) {
    if (arrow) mjx_node_append(node, arrow);
    if (under) mjx_node_append(node, under);
    if (over) mjx_node_append(node, over);
  }
  free(opt);
  free(arg);
  return node;
}

static mjx_node* try_expand_macro(parse_state* state, mjx_parser* parser, const char* cmd) {
  if (!parser || !parser->opts.macros) return NULL;
  for (size_t i = 0; i < parser->opts.macro_count; i++) {
    mjx_macro* m = parser->opts.macros[i];
    if (!m || !m->name || strcmp(m->name, cmd) != 0) continue;
    if (m->is_primitive) return NULL;

    char* expanded = strdup(m->replacement);
    if (!expanded) return NULL;

    for (int a = 1; a <= m->arg_count; a++) {
      char* arg_text = read_argument(state);
      if (!arg_text) { free(expanded); return NULL; }
      char placeholder[8];
      snprintf(placeholder, sizeof(placeholder), "#%d", a);
      char* pos;
      while ((pos = strstr(expanded, placeholder)) != NULL) {
        size_t before = pos - expanded;
        size_t alen = strlen(arg_text);
        size_t plen = strlen(placeholder);
        size_t new_len = before + alen + strlen(pos + plen) + 1;
        char* new_str = (char*)malloc(new_len);
        if (!new_str) { free(expanded); free(arg_text); return NULL; }
        memcpy(new_str, expanded, before);
        memcpy(new_str + before, arg_text, alen);
        strcpy(new_str + before + alen, pos + plen);
        free(expanded);
        expanded = new_str;
      }
      free(arg_text);
    }

    parse_state sub_state;
    memset(&sub_state, 0, sizeof(sub_state));
    sub_state.input = expanded;
    sub_state.pos = expanded;
    sub_state.display = state->display;
    sub_state.style_level = state->style_level;

    mjx_node* result = parse_expression(&sub_state, 0);
    free(expanded);
    return result;
  }
  return NULL;
}

static mjx_node* handle_command(parse_state* state, const char* cmd) {
  uint32_t cp;
  int tex_class;
  int is_operator;

  if (strcmp(cmd, ",") == 0) return make_space_node("0.1667em");
  if (strcmp(cmd, ":") == 0) return make_space_node("0.2222em");
  if (strcmp(cmd, ";") == 0) return make_space_node("0.2778em");
  if (strcmp(cmd, "!") == 0) return make_space_node("-0.1667em");
  if (strcmp(cmd, " ") == 0) return make_space_node("0.3333em");
  if (strcmp(cmd, "quad") == 0) return make_space_node("1em");
  if (strcmp(cmd, "qquad") == 0) return make_space_node("2em");
  if (strcmp(cmd, "enspace") == 0) return make_space_node("0.5em");
  if (strcmp(cmd, "thinspace") == 0) return make_space_node("0.1667em");
  if (strcmp(cmd, "medspace") == 0) return make_space_node("0.2222em");
  if (strcmp(cmd, "thickspace") == 0) return make_space_node("0.2778em");
  if (strcmp(cmd, "negthinspace") == 0) return make_space_node("-0.1667em");
  if (strcmp(cmd, "negmedspace") == 0) return make_space_node("-0.2222em");
  if (strcmp(cmd, "negthickspace") == 0) return make_space_node("-0.2778em");
  if (strcmp(cmd, "hfill") == 0 || strcmp(cmd, "hfil") == 0 ||
      strcmp(cmd, "nobreakspace") == 0) return make_space_node("1em");
  if (strcmp(cmd, "hskip") == 0 || strcmp(cmd, "kern") == 0 ||
      strcmp(cmd, "mkern") == 0 || strcmp(cmd, "mskip") == 0) {
    char* width = read_argument(state);
    mjx_node* node = make_space_node(width ? width : "0em");
    free(width);
    return node;
  }
  if (strcmp(cmd, "allowbreak") == 0) return make_space_node("0em");

  if (mjx_lookup_greek(cmd, &cp, &tex_class))
    return make_operator_node(state, cp, tex_class, 0);

  if (mjx_lookup_binop(cmd, &cp, &tex_class))
    return make_operator_node(state, cp, tex_class, 0);

  if (mjx_lookup_relation(cmd, &cp, &tex_class))
    return make_operator_node(state, cp, tex_class, 0);

  if (mjx_lookup_largeop(cmd, &cp, &tex_class, &is_operator)) {
    if (cp == 0) {
      mjx_node* node = mjx_node_create(MJX_NODE_MO);
      if (node) {
        const char* text_name = largeop_text_name(cmd);
        node->text = strdup(text_name);
        node->text_len = strlen(text_name);
        node->tex_class = MJX_TEXCLASS_OP;
        node->mo_largeop = 0;
        if (largeop_has_movable_limits(cmd)) {
          node->mo_movablelimits = 1;
        }
      }
      return node;
    }
    return make_operator_node(state, cp, tex_class, is_operator);
  }

  if (mjx_lookup_misc(cmd, &cp, &tex_class))
    return make_operator_node(state, cp, tex_class, 0);

  if (strcmp(cmd, "frac") == 0 || strcmp(cmd, "cfrac") == 0) return handle_frac(state);
  if (strcmp(cmd, "sqrt") == 0 || strcmp(cmd, "root") == 0) return handle_sqrt(state, cmd);
  if (strcmp(cmd, "left") == 0 || strcmp(cmd, "right") == 0) return handle_left_right(state, cmd);
  if (strcmp(cmd, "middle") == 0) return handle_middle(state);

  if (strcmp(cmd, "operatorname") == 0) {
    skip_spaces(state);
    int starred = (*state->pos == '*');
    if (starred) state->pos++;
    mjx_node* result = handle_operatorname(state);
    if (result && starred) result->mo_movablelimits = 0;
    return result;
  }
  if (strcmp(cmd, "DeclareMathOperator") == 0) {
    skip_spaces(state);
    int starred = (*state->pos == '*');
    if (starred) state->pos++;

    char* cmd_name = read_argument(state);
    if (!cmd_name) return NULL;
    char* op_text = read_argument(state);
    if (!op_text) { free(cmd_name); return NULL; }

    const char* macro_name = cmd_name;
    if (macro_name[0] == '\\') macro_name++;

    char* replacement = (char*)malloc(strlen(op_text) + 32);
    if (starred) {
      sprintf(replacement, "\\operatorname*{%s}", op_text);
    } else {
      sprintf(replacement, "\\operatorname{%s}", op_text);
    }
    mjx_parser_register_macro(state->parser_ref, macro_name, 0, replacement);

    free(cmd_name);
    free(op_text);
    free(replacement);
    return NULL;
  }
  if (strcmp(cmd, "genfrac") == 0) return handle_genfrac(state);
  if (strcmp(cmd, "stackrel") == 0) return handle_overset_underset(state, 1);
  if (strcmp(cmd, "underbrace") == 0) return handle_braceop(state, 1);
  if (strcmp(cmd, "overbrace") == 0) return handle_braceop(state, 0);
  if (strcmp(cmd, "overleftarrow") == 0) return handle_overunder_accent(state, 0x2190, 0);
  if (strcmp(cmd, "overrightarrow") == 0) return handle_overunder_accent(state, 0x2192, 0);
  if (strcmp(cmd, "overleftrightarrow") == 0) return handle_overunder_accent(state, 0x2194, 0);
  if (strcmp(cmd, "underleftarrow") == 0) return handle_overunder_accent(state, 0x2190, 1);
  if (strcmp(cmd, "underrightarrow") == 0) return handle_overunder_accent(state, 0x2192, 1);
  if (strcmp(cmd, "underleftrightarrow") == 0) return handle_overunder_accent(state, 0x2194, 1);
  if (strcmp(cmd, "overbracket") == 0) return handle_overunder_accent(state, 0x23B4, 0);
  if (strcmp(cmd, "underbracket") == 0) return handle_overunder_accent(state, 0x23B5, 1);
  if (strcmp(cmd, "overparen") == 0) return handle_overunder_accent(state, 0x23DC, 0);
  if (strcmp(cmd, "underparen") == 0) return handle_overunder_accent(state, 0x23DD, 1);
  if (strcmp(cmd, "overset") == 0) return handle_overset_underset(state, 1);
  if (strcmp(cmd, "underset") == 0) return handle_overset_underset(state, 0);
  if (strcmp(cmd, "overline") == 0) return handle_overline_underline(state, 1);
  if (strcmp(cmd, "underline") == 0) return handle_overline_underline(state, 0);
  if (strcmp(cmd, "boxed") == 0 || strcmp(cmd, "fbox") == 0 ||
      strcmp(cmd, "framebox") == 0) return handle_boxed(state);
  if (strcmp(cmd, "color") == 0 || strcmp(cmd, "textcolor") == 0) return handle_color(state);
  if (strcmp(cmd, "hspace") == 0) return handle_hspace(state);
  if (strcmp(cmd, "phantom") == 0) return handle_phantom(state);
  if (strcmp(cmd, "hphantom") == 0 || strcmp(cmd, "vphantom") == 0) return handle_phantom(state);
  if (strcmp(cmd, "smash") == 0) return handle_smash(state);
  if (strcmp(cmd, "hbox") == 0 || strcmp(cmd, "makebox") == 0 ||
      strcmp(cmd, "mathmbox") == 0 || strcmp(cmd, "mathmakebox") == 0 ||
      strcmp(cmd, "textrm") == 0 ||
      strcmp(cmd, "textbf") == 0 || strcmp(cmd, "textsf") == 0 ||
      strcmp(cmd, "texttt") == 0 || strcmp(cmd, "textsc") == 0 ||
      strcmp(cmd, "textsl") == 0 || strcmp(cmd, "textmd") == 0) {
    return handle_text_like(state);
  }
  if (strcmp(cmd, "mathchoice") == 0) return handle_mathchoice(state);
  if (strcmp(cmd, "raisebox") == 0) return handle_raisebox(state);
  if (strcmp(cmd, "rule") == 0) return handle_rule_cmd(state);
  if (strcmp(cmd, "buildrel") == 0) return handle_buildrel(state);
  if (strcmp(cmd, "prescript") == 0) return handle_prescript(state);
  if (strcmp(cmd, "sideset") == 0) return handle_sideset(state);
  if (strcmp(cmd, "cancelto") == 0) return handle_cancelto(state);
  if (strcmp(cmd, "colorbox") == 0) return handle_colorbox(state, 0);
  if (strcmp(cmd, "fcolorbox") == 0) return handle_colorbox(state, 1);
  if (strcmp(cmd, "bbox") == 0) return handle_bbox(state);
  if (strcmp(cmd, "definecolor") == 0) return handle_definecolor(state);
  if (strcmp(cmd, "displaylines") == 0) return handle_displaylines(state);
  if (strcmp(cmd, "ref") == 0) return handle_ref(state);
  if (strcmp(cmd, "strut") == 0) return handle_strut(state);
  if (strcmp(cmd, "iddots") == 0) return handle_iddots(state);
  if (strcmp(cmd, "ddddot") == 0) return handle_dot_accent_text(state, "....");
  if (strcmp(cmd, "mathclap") == 0 || strcmp(cmd, "mathllap") == 0 ||
      strcmp(cmd, "mathrlap") == 0 || strcmp(cmd, "rlap") == 0 ||
      strcmp(cmd, "llap") == 0) return parse_argument_node(state);

  if (strcmp(cmd, "xrightarrow") == 0) return handle_extensible_arrow(state, 0x2192);
  if (strcmp(cmd, "xleftarrow") == 0) return handle_extensible_arrow(state, 0x2190);
  if (strcmp(cmd, "xmapsto") == 0) return handle_extensible_arrow(state, 0x21A6);
  if (strcmp(cmd, "xRightarrow") == 0) return handle_extensible_arrow(state, 0x21D2);
  if (strcmp(cmd, "xLeftarrow") == 0) return handle_extensible_arrow(state, 0x21D0);
  if (strcmp(cmd, "xleftrightarrow") == 0) return handle_extensible_arrow(state, 0x2194);
  if (strcmp(cmd, "xLeftrightarrow") == 0) return handle_extensible_arrow(state, 0x21D4);
  if (strcmp(cmd, "xhookrightarrow") == 0) return handle_extensible_arrow(state, 0x21AA);
  if (strcmp(cmd, "xhookleftarrow") == 0) return handle_extensible_arrow(state, 0x21A9);
  if (strcmp(cmd, "xrightharpoondown") == 0) return handle_extensible_arrow(state, 0x21C1);
  if (strcmp(cmd, "xleftharpoondown") == 0) return handle_extensible_arrow(state, 0x21BD);
  if (strcmp(cmd, "xrightharpoonup") == 0) return handle_extensible_arrow(state, 0x21C0);
  if (strcmp(cmd, "xleftharpoonup") == 0) return handle_extensible_arrow(state, 0x21BC);
  if (strcmp(cmd, "xrightleftharpoons") == 0) return handle_extensible_arrow(state, 0x21CC);
  if (strcmp(cmd, "xleftrightharpoons") == 0) return handle_extensible_arrow(state, 0x21CB);
  if (strcmp(cmd, "xtwoheadleftarrow") == 0) return handle_extensible_arrow(state, 0x219E);
  if (strcmp(cmd, "xtwoheadrightarrow") == 0) return handle_extensible_arrow(state, 0x21A0);

  if (strcmp(cmd, "cancel") == 0 || strcmp(cmd, "bcancel") == 0 || strcmp(cmd, "xcancel") == 0) {
    char* arg = read_argument(state);
    if (!arg) return NULL;
    mjx_node* node = mjx_node_create(MJX_NODE_MENCLOSE);
    if (node) {
      parse_state s = *state; s.input = arg; s.pos = arg;
      mjx_node* content = parse_expression(&s, 0);
      if (content) mjx_node_append(node, content);
      mjx_node_set_attr(node, "notation", cmd);
    }
    free(arg);
    return node;
  }

  if (strcmp(cmd, "boldsymbol") == 0 || strcmp(cmd, "mathbf") == 0) {
    char* arg = read_argument(state);
    if (!arg) return NULL;
    mjx_node* node = mjx_node_create(MJX_NODE_MSTYLE);
    if (node) {
      parse_state s = *state; s.input = arg; s.pos = arg;
      mjx_node* content = parse_expression(&s, 0);
      if (content) mjx_node_append(node, content);
    }
    free(arg);
    return node;
  }

  if (strcmp(cmd, "pmb") == 0 || strcmp(cmd, "mathds") == 0 ||
      strcmp(cmd, "Bbb") == 0 || strcmp(cmd, "frak") == 0 ||
      strcmp(cmd, "mathbfit") == 0 || strcmp(cmd, "mathbfsf") == 0 ||
      strcmp(cmd, "mathbfsc") == 0 || strcmp(cmd, "mathbfcal") == 0) {
    return parse_argument_node(state);
  }

  if (strcmp(cmd, "textit") == 0) {
    char* arg = read_argument(state);
    if (!arg) return NULL;
    mjx_node* node = mjx_node_create(MJX_NODE_MI);
    if (node) { node->text = strdup(arg); node->text_len = strlen(arg); }
    free(arg);
    return node;
  }

  if (strcmp(cmd, "mathrm") == 0) {
    char* arg = read_argument(state);
    if (!arg) return NULL;
    mjx_node* node = mjx_node_create(MJX_NODE_MI);
    if (node) { node->text = strdup(arg); node->text_len = strlen(arg); }
    free(arg);
    return node;
  }

  if (strcmp(cmd, "bf") == 0 || strcmp(cmd, "it") == 0 ||
      strcmp(cmd, "cal") == 0 || strcmp(cmd, "rm") == 0 ||
      strcmp(cmd, "sf") == 0 || strcmp(cmd, "tt") == 0 ||
      strcmp(cmd, "mit") == 0 || strcmp(cmd, "oldstyle") == 0) {
    return NULL;
  }

  if (strcmp(cmd, "bigl") == 0 || strcmp(cmd, "bigr") == 0 ||
      strcmp(cmd, "biggl") == 0 || strcmp(cmd, "biggr") == 0 ||
      strcmp(cmd, "Bigl") == 0 || strcmp(cmd, "Bigr") == 0 ||
      strcmp(cmd, "Biggl") == 0 || strcmp(cmd, "Biggr") == 0 ||
      strcmp(cmd, "bigm") == 0 || strcmp(cmd, "biggm") == 0 ||
      strcmp(cmd, "big") == 0 || strcmp(cmd, "Big") == 0 ||
      strcmp(cmd, "bigg") == 0 || strcmp(cmd, "Bigg") == 0) {
    /* Read the next token as the delimiter */
    skip_spaces(state);
    char* delim = read_argument(state);
    if (!delim) return NULL;
    const char* unicode = mjx_lookup_delimiter(delim);
    mjx_node* node = mjx_node_create(MJX_NODE_MO);
    if (node) {
      if (unicode) {
        node->text = strdup(unicode);
        node->text_len = strlen(unicode);
      } else {
        node->text = strdup(delim);
        node->text_len = strlen(delim);
      }
    }
    free(delim);
    return node;
  }

  if (strcmp(cmd, "(") == 0) return make_operator_str(state, "(");
  if (strcmp(cmd, ")") == 0) return make_operator_str(state, ")");
  if (strcmp(cmd, "[") == 0) return make_operator_str(state, "[");
  if (strcmp(cmd, "]") == 0) return make_operator_str(state, "]");

  if (strcmp(cmd, "{") == 0) return make_operator_str(state, "{");
  if (strcmp(cmd, "}") == 0) return make_operator_str(state, "}");
  if (strcmp(cmd, "|") == 0) return make_operator_node(state, 0x2016, MJX_TEXCLASS_ORD, 0);
  if (strcmp(cmd, ".") == 0) return make_operator_str(state, ".");

  if (mjx_lookup_accent(cmd, &cp)) {
    switch (cp) {
      case 0x0302: cp = 0x02C6; break; /* ^ */
      case 0x0303: cp = 0x02DC; break; /* ~ */
      case 0x0304: cp = 0x00AF; break; /* macron */
      case 0x0307: cp = 0x02D9; break; /* dot */
      case 0x0308: cp = 0x00A8; break; /* dieresis */
      case 0x20D7: cp = 0x20D7; break; /* vec */
      default: break;
    }
    char* arg = read_argument(state);
    if (!arg) return NULL;
    parse_state as = *state; as.input = arg; as.pos = arg;
    mjx_node* arg_node = parse_expression(&as, 0);
    mjx_node* accent = mjx_node_create(MJX_NODE_MOVER);
    if (accent && arg_node) {
      mjx_node_append(accent, arg_node);
      mjx_node* accent_mo = make_operator_node(state, cp, MJX_TEXCLASS_ORD, 0);
      if (accent_mo) mjx_node_append(accent, accent_mo);
    }
    free(arg);
    return accent;
  }

  if (strcmp(cmd, "substack") == 0) {
    char* arg = read_argument(state);
    if (!arg) return NULL;
    mjx_node* table = mjx_node_create(MJX_NODE_MTABLE);
    if (table) {
      char* p = arg;
      while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        char* row_start = p;
        char* row_end = NULL;
        while (*p) {
          if (*p == '\\' && *(p+1) == '\\') {
            row_end = p;
            p += 2;
            break;
          }
          p++;
        }
        if (!row_end) row_end = p;
        if (row_end > row_start) {
          size_t row_len = (size_t)(row_end - row_start);
          char* row_text = (char*)malloc(row_len + 1);
          if (row_text) {
            memcpy(row_text, row_start, row_len);
            row_text[row_len] = '\0';
            mjx_node* mtr = mjx_node_create(MJX_NODE_MTR);
            mjx_node* mtd = mjx_node_create(MJX_NODE_MTD);
            if (mtr && mtd) {
              parse_state rs = *state; rs.input = row_text; rs.pos = row_text;
              mjx_node* expr = parse_expression(&rs, 0);
              if (expr) mjx_node_append(mtd, expr);
              mjx_node_append(mtr, mtd);
              mjx_node_append(table, mtr);
            }
            free(row_text);
          }
        }
      }
    }
    free(arg);
    mjx_node* style = mjx_node_create(MJX_NODE_MSTYLE);
    if (style) {
      mjx_node_set_attr(style, "displaystyle", "false");
      mjx_node_set_attr(style, "scriptlevel", "1");
      if (table) mjx_node_append(style, table);
    }
    return style ? style : table;
  }

  if (strcmp(cmd, "not") == 0) {
    const char* saved = state->pos;
    skip_spaces(state);
    uint32_t cp = 0;
    if (*state->pos == '\\') {
      const char* cs_start = state->pos + 1;
      const char* cs_end = cs_start;
      while (isalpha((unsigned char)*cs_end)) cs_end++;
      size_t cs_len = (size_t)(cs_end - cs_start);
      if (cs_len > 0) {
        char* cs = (char*)malloc(cs_len + 1);
        if (cs) {
          memcpy(cs, cs_start, cs_len); cs[cs_len] = '\0';
          uint32_t base_cp = 0; int tc;
          if (mjx_lookup_relation(cs, &base_cp, &tc) && base_cp) {
            static const struct { uint32_t from; uint32_t to; } negate_map[] = {
              {0x2261,0x2262},{0x2282,0x2284},{0x2283,0x2285},{0x2286,0x2288},
              {0x2287,0x2289},{0x220B,0x220C},{0x2264,0x2270},{0x2265,0x2271},
              {0x2248,0x2249},{0x223C,0x2241},{0x2245,0x2247},{0x2250,0x2251},
              {0x2272,0x2274},{0x2273,0x2275},{0x2276,0x2278},{0x2277,0x2279},
              {0x227A,0x2280},{0x227B,0x2281},{0x227C,0x22E0},{0x227D,0x22E1},
              {0x224D,0x226D},{0x2243,0x2244},{0x2246,0x2247},{0x2266,0x2278},
              {0x2267,0x2279},{0x226A,0x2276},{0x226B,0x2277},
            };
            for (size_t i = 0; i < sizeof(negate_map)/sizeof(negate_map[0]); i++) {
              if (negate_map[i].from == base_cp) { cp = negate_map[i].to; break; }
            }
          }
          free(cs);
        }
      }
    } else if (*state->pos == '=') {
      cp = 0x2260;
    } else if (*state->pos == '<') {
      cp = 0x226E;
    } else if (*state->pos == '>') {
      cp = 0x226F;
    } else if (*state->pos == '|') {
      cp = 0x2224;
    }
    if (cp) {
      state->pos = saved;
      skip_spaces(state);
      if (*state->pos == '\\') {
        const char* cs_start = state->pos + 1;
        while (isalpha((unsigned char)*cs_start)) cs_start++;
        state->pos = cs_start;
      } else {
        state->pos++;
      }
      char utf8[5]; int n = 0;
      if (cp < 0x80) { utf8[n++] = (char)cp; }
      else if (cp < 0x800) { utf8[n++] = 0xC0 | (cp >> 6); utf8[n++] = 0x80 | (cp & 0x3F); }
      else { utf8[n++] = 0xE0 | (cp >> 12); utf8[n++] = 0x80 | ((cp >> 6) & 0x3F); utf8[n++] = 0x80 | (cp & 0x3F); }
      utf8[n] = '\0';
      mjx_node* node = mjx_node_create(MJX_NODE_MO);
      if (node) { node->text = strdup(utf8); node->text_len = n; }
      return node;
    }
    state->pos = saved;
    mjx_node* next = parse_group(state);
    if (!next) return NULL;
    return next;
  }

  if (strcmp(cmd, "text") == 0 || strcmp(cmd, "mbox") == 0) {
    char* arg = read_argument(state);
    if (!arg) return NULL;
    mjx_node* node = mjx_node_create(MJX_NODE_MTEXT);
    if (node) { node->text = strdup(arg); node->text_len = strlen(arg); }
    free(arg);
    return node;
  }

  if (strcmp(cmd, "pmod") == 0) {
    char* arg = read_argument(state);
    if (!arg) return NULL;
    mjx_node* row = mjx_node_create(MJX_NODE_MROW);
    if (row) {
      mjx_node* lp = mjx_node_create(MJX_NODE_MO);
      if (lp) { lp->text = strdup("("); lp->text_len = 1; mjx_node_append(row, lp); }
      mjx_node* mod = mjx_node_create(MJX_NODE_MO);
      if (mod) { mod->text = strdup("mod"); mod->text_len = 3; mjx_node_append(row, mod); }
      mjx_node* space = mjx_node_create(MJX_NODE_MSPACE);
      if (space) { mjx_node_set_attr(space, "width", "0.2778em"); mjx_node_append(row, space); }
      parse_state s = *state; s.input = arg; s.pos = arg;
      mjx_node* content = parse_expression(&s, 0);
      if (content) mjx_node_append(row, content);
      mjx_node* rp = mjx_node_create(MJX_NODE_MO);
      if (rp) { rp->text = strdup(")"); rp->text_len = 1; mjx_node_append(row, rp); }
    }
    free(arg);
    return row;
  }
  if (strcmp(cmd, "bmod") == 0) {
    mjx_node* node = mjx_node_create(MJX_NODE_MO);
    if (node) { node->text = strdup("mod"); node->text_len = 3; node->tex_class = MJX_TEXCLASS_BIN; }
    return node;
  }
  if (strcmp(cmd, "mod") == 0) {
    char* arg = read_argument(state);
    if (!arg) return NULL;
    mjx_node* row = mjx_node_create(MJX_NODE_MROW);
    if (row) {
      mjx_node* space = mjx_node_create(MJX_NODE_MSPACE);
      if (space) { mjx_node_set_attr(space, "width", "0.2778em"); mjx_node_append(row, space); }
      mjx_node* mod = mjx_node_create(MJX_NODE_MO);
      if (mod) { mod->text = strdup("mod"); mod->text_len = 3; mjx_node_append(row, mod); }
      mjx_node* space2 = mjx_node_create(MJX_NODE_MSPACE);
      if (space2) { mjx_node_set_attr(space2, "width", "0.2778em"); mjx_node_append(row, space2); }
      parse_state s = *state; s.input = arg; s.pos = arg;
      mjx_node* content = parse_expression(&s, 0);
      if (content) mjx_node_append(row, content);
    }
    free(arg);
    return row;
  }

  if (strcmp(cmd, "mathbf") == 0 || strcmp(cmd, "mathit") == 0 ||
      strcmp(cmd, "mathrm") == 0 || strcmp(cmd, "mathcal") == 0 ||
      strcmp(cmd, "mathbb") == 0 || strcmp(cmd, "mathsf") == 0 ||
      strcmp(cmd, "mathtt") == 0 || strcmp(cmd, "mathfrak") == 0 ||
      strcmp(cmd, "mathscr") == 0) {
    char* arg = read_argument(state);
    if (!arg) return NULL;
    parse_state sub = *state;
    sub.input = arg; sub.pos = arg;
    mjx_node* content = parse_expression(&sub, 0);
    free(arg);
    return content ? content : mjx_node_create(MJX_NODE_MROW);
  }

  if (strcmp(cmd, "displaystyle") == 0) { state->style_level = 0; return NULL; }
  if (strcmp(cmd, "textstyle") == 0) { state->style_level = 0; return NULL; }
  if (strcmp(cmd, "scriptstyle") == 0) { state->style_level = 1; return NULL; }
  if (strcmp(cmd, "scriptscriptstyle") == 0) { state->style_level = 2; return NULL; }
  if (strcmp(cmd, "limits") == 0) return NULL;
  if (strcmp(cmd, "nolimits") == 0) return NULL;

  { const char* delim_unicode = NULL;
    if (strlen(cmd) > 1) delim_unicode = mjx_lookup_delimiter(cmd);
    if (delim_unicode) {
      mjx_node* node = mjx_node_create(MJX_NODE_MO);
      if (node) { node->text = strdup(delim_unicode); node->text_len = strlen(delim_unicode); }
      return node;
    }
  }

  if (strcmp(cmd, "begin") == 0) {
    char* env_name = read_argument(state);
    if (!env_name) return NULL;

    if (strcmp(env_name, "array") == 0) {
      skip_spaces(state);
      if (*state->pos == '[') {
        int depth = 1;
        state->pos++;
        while (*state->pos && depth > 0) {
          if (*state->pos == '[') depth++;
          else if (*state->pos == ']') depth--;
          state->pos++;
        }
        skip_spaces(state);
      }
      if (*state->pos == '{') {
        char* colspec = read_argument(state);
        free(colspec);
      }
    }

    mjx_node* table = mjx_node_create(MJX_NODE_MTABLE);
    mjx_node* cur_tr = mjx_node_create(MJX_NODE_MTR);
    mjx_node* cur_td_content = mjx_node_create(MJX_NODE_MROW);

    int done = 0;
    while (!done && *state->pos) {
      if (*state->pos == '\\' && *(state->pos + 1) == '\\') {
        state->pos += 2;
        mjx_node* td = mjx_node_create(MJX_NODE_MTD);
        mjx_node_append(td, cur_td_content);
        mjx_node_append(cur_tr, td);
        mjx_node_append(table, cur_tr);
        cur_tr = mjx_node_create(MJX_NODE_MTR);
        cur_td_content = mjx_node_create(MJX_NODE_MROW);
        continue;
      }

      if (*state->pos == '&') {
        state->pos++;
        mjx_node* td = mjx_node_create(MJX_NODE_MTD);
        mjx_node_append(td, cur_td_content);
        mjx_node_append(cur_tr, td);
        cur_td_content = mjx_node_create(MJX_NODE_MROW);
        continue;
      }

      if (*state->pos == '\\') {
        const char* saved = state->pos;
        state->pos++;
        char end_buf[256];
        size_t ei = 0;
        if (isalpha((unsigned char)*state->pos)) {
          while (isalpha((unsigned char)*state->pos) && ei < 255) {
            end_buf[ei++] = *state->pos++;
          }
          end_buf[ei] = '\0';
          if (strcmp(end_buf, "end") == 0) {
            skip_spaces(state);
            if (*state->pos == '{') {
              state->pos++;
              char end_name[256];
              size_t ni = 0;
              while (*state->pos && *state->pos != '}' && ni < 255) {
                end_name[ni++] = *state->pos++;
              }
              end_name[ni] = '\0';
              if (*state->pos == '}') state->pos++;

              if (strcmp(end_name, env_name) == 0) {
                done = 1;
                mjx_node* td = mjx_node_create(MJX_NODE_MTD);
                mjx_node_append(td, cur_td_content);
                mjx_node_append(cur_tr, td);
                mjx_node_append(table, cur_tr);
              } else {
                state->pos = saved;
              }
            }
          } else {
            state->pos = saved;
          }
        } else {
          state->pos = saved;
        }
      }

      if (!done) {
        skip_spaces(state);
        /* Scan ahead to find end of cell: \\, &, or \end{env_name} */
        /* Track brace depth to avoid stopping inside nested groups */
        const char* cell_start_pos = state->pos;
        const char* cell_end_pos = state->pos;
        int brace_depth = 0;
        while (*cell_end_pos) {
          if (*cell_end_pos == '{') { brace_depth++; cell_end_pos++; continue; }
          if (*cell_end_pos == '}') { brace_depth--; cell_end_pos++; continue; }
          if (brace_depth == 0) {
            if (*cell_end_pos == '\\' && *(cell_end_pos + 1) == '\\') break;
            if (*cell_end_pos == '&') break;
            if (*cell_end_pos == '\\') {
              const char* p = cell_end_pos + 1;
              if (isalpha((unsigned char)*p)) {
                const char* q = p;
                while (isalpha((unsigned char)*q)) q++;
                size_t cmd_len = (size_t)(q - p);
                if (cmd_len == 3 && strncmp(p, "end", 3) == 0) {
                  const char* ep = q;
                  while (*ep == ' ' || *ep == '\t') ep++;
                  if (*ep == '{') {
                    ep++;
                    const char* en = ep;
                    while (*en && *en != '}') en++;
                    size_t env_len = (size_t)(en - ep);
                    size_t match_len = strlen(env_name);
                    if (env_len == match_len && strncmp(ep, env_name, match_len) == 0) break;
                  }
                }
              }
            }
          }
          cell_end_pos++;
        }

        if (cell_end_pos > cell_start_pos) {
          char* cell_text = (char*)malloc((size_t)(cell_end_pos - cell_start_pos) + 1);
          if (cell_text) {
            memcpy(cell_text, cell_start_pos, (size_t)(cell_end_pos - cell_start_pos));
            cell_text[cell_end_pos - cell_start_pos] = '\0';
            parse_state sub_state = *state;
            sub_state.input = cell_text;
            sub_state.pos = cell_text;
            mjx_node* expr = parse_expression(&sub_state, 0);
            if (expr) mjx_node_append(cur_td_content, expr);
            free(cell_text);
          }
        }
        state->pos = cell_end_pos;
      }
    }

    mjx_node_set_attr(table, "env", env_name);

    /* Dispatch to registered env handler if one exists */
    mjx_env_handler handler = NULL;
    mjx_parser* p = state->parser_ref;
    for (size_t ei = 0; ei < p->opts.env_count; ei++) {
      if (p->opts.env_names[ei] &&
          strcmp(p->opts.env_names[ei], env_name) == 0) {
        handler = p->opts.env_handlers[ei];
        break;
      }
    }

    mjx_node* result;
    if (handler) {
      result = handler(p, env_name, table);
    } else {
      result = table;
    }

    free(env_name);
    return result;
  }

  { mjx_node* expanded = try_expand_macro(state, state->parser_ref, cmd);
    if (expanded) return expanded; }

  {
    mjx_node* node = mjx_node_create(MJX_NODE_MI);
    if (node) {
      char* full = (char*)malloc(strlen(cmd) + 2);
      if (full) {
        full[0] = '\\';
        strcpy(full + 1, cmd);
        node->text = full;
        node->text_len = strlen(full);
      }
    }
    return node;
  }
}

static mjx_node* parse_group(parse_state* state) {
  skip_spaces(state);

  if (*state->pos == '{') {
    state->pos++;
    mjx_node* content = parse_expression(state, 1);
    if (*state->pos == '}') state->pos++;
    if (content) {
      content = unwrap_mrow(content);
      mjx_node* with_scripts = handle_scripts(state, content);
      if (with_scripts) return with_scripts;
    }
    return content;
  }

  if (*state->pos == '\\') {
    char* cs = read_cs(state);
    if (!cs) return NULL;
    if (strcmp(cs, "limits") == 0 || strcmp(cs, "nolimits") == 0) {
      state->limits_mode = (strcmp(cs, "limits") == 0) ? 1 : -1;
      free(cs);
      return parse_group(state);
    }
    mjx_node* result = handle_command(state, cs);
    free(cs);
    if (result) {
      mjx_node* with_scripts = handle_scripts(state, result);
      if (with_scripts) return with_scripts;
      return result;
    }
    /* NULL from style commands (\displaystyle, etc.) — retry */
    return parse_group(state);
  }

  const char* start = state->pos;
  size_t len = 0;
  while (*state->pos && *state->pos != '\\' && *state->pos != '{' &&
         *state->pos != '}' && *state->pos != '^' && *state->pos != '_' &&
         *state->pos != '&' && *state->pos != '$' &&
         !isspace((unsigned char)*state->pos) &&
         !is_operator_char((unsigned char)*state->pos)) {
    len++;
    state->pos++;
  }

  if (len == 0 && is_operator_char((unsigned char)*state->pos)) {
    char single[2] = { *state->pos++, '\0' };
    mjx_node* node = make_operator_str(state, single);
    mjx_node* with_scripts = handle_scripts(state, node);
    return with_scripts ? with_scripts : node;
  }

  if (len == 0) return NULL;

  char* text = (char*)malloc(len + 1);
  memcpy(text, start, len);
  text[len] = '\0';

  int is_number = 1;
  for (size_t i = 0; i < len; i++) {
    if (!isdigit((unsigned char)text[i]) && text[i] != '.') {
      is_number = 0; break;
    }
  }

  if (len == 1) {
    mjx_node* node;
    if (is_number) node = make_number(state, text);
    else if (is_operator_char((unsigned char)text[0])) node = make_operator_str(state, text);
    else node = make_identifier(state, text);
    free(text);
    mjx_node* with_scripts = handle_scripts(state, node);
    return with_scripts ? with_scripts : node;
  }

  mjx_node* row = mjx_node_create(MJX_NODE_MROW);
  if (!row) { free(text); return NULL; }

  for (size_t i = 0; i < len; i++) {
    char single[2] = { text[i], '\0' };
    unsigned char c = (unsigned char)text[i];
    if (isdigit(c)) {
      mjx_node* mn = make_number(state, single);
      if (mn) mjx_node_append(row, mn);
    } else if (is_operator_char(c)) {
      mjx_node* mo = make_operator_str(state, single);
      if (mo) mjx_node_append(row, mo);
    } else {
      mjx_node* mi = make_identifier(state, single);
      if (mi) mjx_node_append(row, mi);
    }
  }
  free(text);

  mjx_node* with_scripts = handle_scripts(state, row);
  return with_scripts ? with_scripts : row;
}

static int peek_infix_fraction(parse_state* state, char* cmd, size_t cmd_size) {
  if (*state->pos != '\\') return 0;
  const char* p = state->pos + 1;
  size_t i = 0;
  while (isalpha((unsigned char)*p) && i + 1 < cmd_size) {
    cmd[i++] = *p++;
  }
  cmd[i] = '\0';
  if (i == 0) return 0;
  return strcmp(cmd, "over") == 0 ||
         strcmp(cmd, "atop") == 0 ||
         strcmp(cmd, "above") == 0 ||
         strcmp(cmd, "choose") == 0 ||
         strcmp(cmd, "brack") == 0 ||
         strcmp(cmd, "brace") == 0 ||
         strcmp(cmd, "atopwithdelims") == 0 ||
         strcmp(cmd, "abovewithdelims") == 0;
}

static mjx_node* handle_infix_fraction(parse_state* state, mjx_node* numerator,
                                       const char* cmd, int stop_at_brace) {
  char* read = read_cs(state);
  free(read);

  char* left = NULL;
  char* right = NULL;
  char* thickness = NULL;

  if (strcmp(cmd, "atopwithdelims") == 0 ||
      strcmp(cmd, "abovewithdelims") == 0) {
    left = read_argument(state);
    right = read_argument(state);
  } else if (strcmp(cmd, "choose") == 0) {
    left = strdup("(");
    right = strdup(")");
  } else if (strcmp(cmd, "brack") == 0) {
    left = strdup("[");
    right = strdup("]");
  } else if (strcmp(cmd, "brace") == 0) {
    left = strdup("{");
    right = strdup("}");
  }
  if (strcmp(cmd, "above") == 0 ||
      strcmp(cmd, "abovewithdelims") == 0) {
    thickness = read_argument(state);
  }

  mjx_node* denominator = parse_expression(state, stop_at_brace);
  if (!denominator) denominator = mjx_node_create(MJX_NODE_MROW);

  mjx_node* frac = mjx_node_create(MJX_NODE_MFRAC);
  if (!frac) {
    if (denominator) mjx_node_destroy(denominator);
    free(left); free(right); free(thickness);
    return numerator;
  }
  mjx_node_append(frac, numerator);
  mjx_node_append(frac, denominator);
  if (strcmp(cmd, "atop") == 0 || strcmp(cmd, "atopwithdelims") == 0 ||
      strcmp(cmd, "choose") == 0 || strcmp(cmd, "brack") == 0 ||
      strcmp(cmd, "brace") == 0) {
    mjx_node_set_attr(frac, "linethickness", "0pt");
  } else if (thickness && thickness[0]) {
    mjx_node_set_attr(frac, "linethickness", thickness);
  }

  if (left && right) {
    mjx_node* wrapped = mjx_node_create(MJX_NODE_MROW);
    if (wrapped) {
      mjx_node_set_attr(wrapped, "left", left);
      mjx_node_set_attr(wrapped, "right", right);
      mjx_node_set_attr(wrapped, "fixed_delims", "true");
      mjx_node_append(wrapped, frac);
      free(left); free(right); free(thickness);
      return wrapped;
    }
  }

  free(left); free(right); free(thickness);
  return frac;
}

static mjx_node* parse_expression(parse_state* state, int stop_at_brace) {
  mjx_node* row = mjx_node_create(MJX_NODE_MROW);
  if (!row) return NULL;

  while (*state->pos) {
    skip_spaces(state);
    if (!*state->pos) break;
    if (stop_at_brace && *state->pos == '}') break;
    if (*state->pos == '\\' && strncmp(state->pos, "\\right", 6) == 0 &&
        !isalpha((unsigned char)state->pos[6])) {
      break;
    }
    char infix_cmd[32];
    if (peek_infix_fraction(state, infix_cmd, sizeof(infix_cmd))) {
      return handle_infix_fraction(state, row, infix_cmd, stop_at_brace);
    }
    if (*state->pos == '$') { state->pos++; continue; }

    mjx_node* node = parse_group(state);
    if (node) {
      mjx_node_append(row, node);
    } else {
      break;
    }
  }

  if (row->child_count == 0) return row;
  mjx_node* unwrapped = unwrap_mrow(row);
  if (unwrapped != row) {
    mjx_node_destroy(row);
    return unwrapped;
  }
  return row;
}

mjx_parser* mjx_parser_create(mjx_parse_opts* opts) {
  mjx_parser* parser = (mjx_parser*)calloc(1, sizeof(mjx_parser));
  if (!parser) return NULL;
  if (opts) parser->opts = *opts;
  return parser;
}

void mjx_parser_destroy(mjx_parser* parser) {
  if (!parser) return;
  if (parser->result) mjx_node_destroy(parser->result);
  free(parser);
}

mjx_node* mjx_parser_parse_latex(mjx_parser* parser, const char* latex, int display) {
  if (!parser || !latex) return NULL;

  parse_state state;
  memset(&state, 0, sizeof(state));
  state.input = latex;
  state.pos = latex;
  state.display = display;
  state.style_level = 0;
  state.parser_ref = parser;

  skip_spaces(&state);

  if (*state.pos == '$') {
    state.pos++;
    if (*state.pos == '$') state.pos++;
  }

  mjx_node* result = parse_expression(&state, 0);

  if (*state.pos == '$') state.pos++;
  if (*state.pos == '$') state.pos++;

  if (parser->result) mjx_node_destroy(parser->result);
  parser->result = result;
  return result;
}

mjx_node* mjx_parser_parse_mathml(mjx_parser* parser, const char* xml, int display) {
  if (!parser || !xml) return NULL;

  mjx_node* row = mjx_node_create(MJX_NODE_MROW);
  if (!row) return NULL;

  const char* p = xml;
  const char* tag_start = NULL;
  const char* tag_end = NULL;
  const char* text_start = NULL;
  int in_tag = 0;
  int in_text = 0;
  char tag_name[256];

  while (*p) {
    if (*p == '<') {
      if (in_text) {
        text_start = NULL;
        in_text = 0;
      }
      tag_start = p;
      in_tag = 1;
      p++;
      int is_end = (*p == '/');
      if (is_end) p++;
      size_t ni = 0;
      while (*p && *p != '>' && *p != ' ' && *p != '\t' && *p != '\n' && ni < 255) {
        tag_name[ni++] = *p++;
      }
      tag_name[ni] = '\0';

      if (is_end) {
        while (*p && *p != '>') p++;
        if (*p == '>') p++;
        tag_end = p;
        in_tag = 0;
      } else {
        int self_close = 0;
        while (*p && *p != '>') {
          if (*p == '/') self_close = 1;
          p++;
        }
        if (*p == '>') p++;
        tag_end = p;
        in_tag = 0;
        if (self_close) {
          in_tag = 0;
        }
      }
      continue;
    }

    if (*p == '&') {
      p++;
      char ent[32];
      size_t ei = 0;
      while (*p && *p != ';' && ei < 31) ent[ei++] = *p++;
      ent[ei] = '\0';
      if (*p == ';') p++;
      uint32_t code = 0;
      if (ent[0] == '#') {
        code = atoi(ent + 1);
      } else if (strcmp(ent, "amp") == 0) code = '&';
      else if (strcmp(ent, "lt") == 0) code = '<';
      else if (strcmp(ent, "gt") == 0) code = '>';
      else if (strcmp(ent, "quot") == 0) code = '"';

      if (code) {
        char utf8[8];
        if (code < 0x80) { utf8[0] = code; utf8[1] = 0; }
        else if (code < 0x800) { utf8[0] = 0xC0 | (code >> 6); utf8[1] = 0x80 | (code & 0x3F); utf8[2] = 0; }
        else { utf8[0] = 0xE0 | (code >> 12); utf8[1] = 0x80 | ((code >> 6) & 0x3F); utf8[2] = 0x80 | (code & 0x3F); utf8[3] = 0; }
        mjx_node* mi = mjx_node_create(MJX_NODE_MI);
        if (mi) { mi->text = strdup(utf8); mi->text_len = strlen(utf8); mjx_node_append(row, mi); }
      }
      continue;
    }

    if (*p == '>') { p++; continue; }

    if (isspace((unsigned char)*p) && !in_text) { p++; continue; }

    if (!in_text) {
      text_start = p;
      in_text = 1;
    }
    p++;
  }

  if (parser->result) mjx_node_destroy(parser->result);
  parser->result = row;
  return row;
}

int mjx_parser_register_macro(mjx_parser* parser, const char* name,
                               int arg_count, const char* replacement) {
  if (!parser || !name) return 0;
  mjx_macro* m = mjx_macro_create(name, arg_count, replacement);
  if (!m) return 0;
  size_t new_count = parser->opts.macro_count + 1;
  mjx_macro** new_macros = (mjx_macro**)realloc(parser->opts.macros,
                            new_count * sizeof(mjx_macro*));
  if (!new_macros) { mjx_macro_destroy(m); return 0; }
  parser->opts.macros = new_macros;
  parser->opts.macros[parser->opts.macro_count] = m;
  parser->opts.macro_count = new_count;
  return 1;
}

int mjx_parser_register_env(mjx_parser* parser, const char* name,
                             mjx_env_handler handler) {
  if (!parser || !name || !handler) return 0;
  size_t new_count = parser->opts.env_count + 1;
  mjx_env_handler* new_handlers = (mjx_env_handler*)realloc(
    parser->opts.env_handlers, new_count * sizeof(mjx_env_handler));
  const char** new_names = (const char**)realloc(
    parser->opts.env_names, new_count * sizeof(const char*));
  if (!new_handlers || !new_names) {
    free(new_handlers); free(new_names); return 0;
  }
  parser->opts.env_handlers = new_handlers;
  parser->opts.env_names = new_names;
  parser->opts.env_names[parser->opts.env_count] = strdup(name);
  parser->opts.env_handlers[parser->opts.env_count] = handler;
  parser->opts.env_count = new_count;
  return 1;
}

const char* mjx_parser_error(mjx_parser* parser) {
  if (!parser) return "null parser";
  return parser->error_msg;
}

const mjx_operator* mjx_parser_lookup_operator(mjx_parser* parser, const char* symbol) {
  if (!parser || !parser->opts.operators || !symbol) return NULL;
  for (size_t i = 0; i < parser->opts.operator_count; i++) {
    if (strcmp(parser->opts.operators[i].symbol, symbol) == 0)
      return &parser->opts.operators[i];
  }
  return NULL;
}
