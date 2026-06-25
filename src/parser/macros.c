#include "parser.h"
#include <stdlib.h>
#include <string.h>

/* Define a macro */
mjx_macro* mjx_macro_create(const char* name, int arg_count, const char* replacement) {
  mjx_macro* m = (mjx_macro*)calloc(1, sizeof(mjx_macro));
  if (!m) return NULL;
  m->name = strdup(name);
  m->arg_count = arg_count;
  m->replacement = replacement ? strdup(replacement) : NULL;
  return m;
}

void mjx_macro_destroy(mjx_macro* m) {
  if (!m) return;
  free(m->name);
  free(m->replacement);
  free(m);
}

/* Primitive command handler type */
typedef mjx_node* (*mjx_primitive_fn)(mjx_parser* parser);

typedef struct {
  const char* name;
  int arg_count;
  const char* replacement;
  int is_primitive;
} mjx_macro_def;

/* These will be expanded as we add more macros */
static mjx_macro_def BUILTIN_MACROS[] = {
  /* AMS math commands that can be expressed as replacements */
  {"tfrac", 2, "\\frac{#1}{#2}", 0},
  {"dfrac", 2, "\\frac{#1}{#2}", 0},
  {"binom", 2, "\\genfrac{(}{)}{0pt}{}{#1}{#2}", 0},
  {"dbinom", 2, "\\genfrac{(}{)}{0pt}{}{#1}{#2}", 0},
  {"tbinom", 2, "\\genfrac{(}{)}{0pt}{}{#1}{#2}", 0},
  {"choose", 2, "\\genfrac{(}{)}{0pt}{}{#1}{#2}", 0},
  {"brace", 2, "\\genfrac{\\{}{\\}}{0pt}{}{#1}{#2}", 0},
  {"brack", 2, "\\genfrac{[}{]}{0pt}{}{#1}{#2}", 0},
  {"pmod", 1, "\\allowbreak\\kern3mu(\\text{mod}\\kern1mu #1)", 0},
  {"mod", 1, "\\allowbreak\\kern3mu\\text{mod}\\kern1mu #1", 0},
  {"pod", 1, "\\allowbreak\\kern3mu(#1)", 0},
  {"text", 1, "\\mathchoice{\\hbox{#1}}{\\hbox{#1}}{\\hbox{#1}}{\\hbox{#1}}", 0},
  {"mbox", 1, "\\mathchoice{\\hbox{#1}}{\\hbox{#1}}{\\hbox{#1}}{\\hbox{#1}}", 0},
  {"implies", 0, "\\;\\Longrightarrow\\;", 0},
  {"iff", 0, "\\;\\Longleftrightarrow\\;", 0},
  {"impliedby", 0, "\\;\\Longleftarrow\\;", 0},
  {"colon", 0, "\\mathrel{\\mathop\\colon}", 0},
  {"idotsint", 0, "\\int\\!\\int\\!\\int", 0},
  {"iiiint", 0, "\\int\\!\\int\\!\\int\\!\\int", 0},
  {"dddot", 1, "\\mathop{#1}\\limits^{\\text{...}}", 0},
  {"dddot", 1, "\\mathop{#1}\\limits^{\\text{...}}", 0},
};

/* AMS font commands */
static mjx_macro_def FONT_MACROS[] = {
  {"mathbb", 1, "\\hbox{\\mathvariant{double-struck}{#1}}", 0},
  {"mathcal", 1, "\\hbox{\\mathvariant{script}{#1}}", 0},
  {"mathbf", 1, "\\hbox{\\mathvariant{bold}{#1}}", 0},
  {"mathit", 1, "\\hbox{\\mathvariant{italic}{#1}}", 0},
  {"mathrm", 1, "\\hbox{\\mathvariant{normal}{#1}}", 0},
  {"mathsf", 1, "\\hbox{\\mathvariant{sans-serif}{#1}}", 0},
  {"mathtt", 1, "\\hbox{\\mathvariant{monospace}{#1}}", 0},
  {"mathfrak", 1, "\\hbox{\\mathvariant{fraktur}{#1}}", 0},
  {"mathscr", 1, "\\hbox{\\mathvariant{script}{#1}}", 0},
};

mjx_macro** mjx_default_macros(size_t* count) {
  size_t total = sizeof(BUILTIN_MACROS) / sizeof(BUILTIN_MACROS[0])
               + sizeof(FONT_MACROS) / sizeof(FONT_MACROS[0]);
  mjx_macro** macros = (mjx_macro**)calloc(total + 1, sizeof(mjx_macro*));
  if (!macros) return NULL;

  size_t idx = 0;

  for (size_t i = 0; i < sizeof(BUILTIN_MACROS) / sizeof(BUILTIN_MACROS[0]); i++) {
    macros[idx] = mjx_macro_create(BUILTIN_MACROS[i].name,
                                    BUILTIN_MACROS[i].arg_count,
                                    BUILTIN_MACROS[i].replacement);
    macros[idx]->is_primitive = BUILTIN_MACROS[i].is_primitive;
    if (macros[idx]) idx++;
  }

  for (size_t i = 0; i < sizeof(FONT_MACROS) / sizeof(FONT_MACROS[0]); i++) {
    macros[idx] = mjx_macro_create(FONT_MACROS[i].name,
                                    FONT_MACROS[i].arg_count,
                                    FONT_MACROS[i].replacement);
    macros[idx]->is_primitive = FONT_MACROS[i].is_primitive;
    if (macros[idx]) idx++;
  }

  *count = idx;
  return macros;
}
