#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Character classes */
#define CC_SPACE  0
#define CC_LETTER 1
#define CC_OTHER  2
#define CC_ESCAPE 3

/* Simple UTF-8 decoder: returns codepoint and advances pointer */
uint32_t utf8_decode(const char** s) {
  const unsigned char* p = (const unsigned char*)*s;
  if (!p || !*p) return 0;

  uint32_t cp;
  if (p[0] < 0x80) {
    cp = p[0];
    *s += 1;
  } else if ((p[0] & 0xE0) == 0xC0 && p[1]) {
    cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
    *s += 2;
  } else if ((p[0] & 0xF0) == 0xE0 && p[1] && p[2]) {
    cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    *s += 3;
  } else if ((p[0] & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) {
    cp = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
    *s += 4;
  } else {
    cp = p[0];
    *s += 1;
  }
  return cp;
}

/* Get next token from LaTeX string */
typedef struct {
  const char* start;
  const char* pos;
  char token[256];
  int token_type;   /* 0=char, 1=control, 2=begin_group, 3=end_group, 4=alignment, 5=eof */
} mjx_lexer;

static void lexer_init(mjx_lexer* lex, const char* input) {
  lex->start = input;
  lex->pos = input;
  lex->token[0] = '\0';
  lex->token_type = 5;
}

static int lexer_next(mjx_lexer* lex) {
  lex->token[0] = '\0';

  if (!lex->pos || !*lex->pos) {
    lex->token_type = 5; /* EOF */
    return 5;
  }

  /* Skip spaces */
  while (*lex->pos && (*lex->pos == ' ' || *lex->pos == '\t')) {
    lex->pos++;
  }

  if (!*lex->pos) {
    lex->token_type = 5;
    return 5;
  }

  char c = *lex->pos;

  if (c == '{') {
    lex->pos++;
    lex->token[0] = '{';
    lex->token[1] = '\0';
    lex->token_type = 2; /* begin_group */
    return 2;
  }

  if (c == '}') {
    lex->pos++;
    lex->token[0] = '}';
    lex->token[1] = '\0';
    lex->token_type = 3; /* end_group */
    return 3;
  }

  if (c == '&') {
    lex->pos++;
    lex->token[0] = '&';
    lex->token[1] = '\0';
    lex->token_type = 4; /* alignment */
    return 4;
  }

  if (c == '\\') {
    lex->pos++;
    lex->token[0] = '\\';

    if (!*lex->pos) {
      lex->token[1] = '\0';
      lex->token_type = 1;
      return 1;
    }

    /* Control symbol (non-letter) */
    if (!((*lex->pos >= 'a' && *lex->pos <= 'z') ||
          (*lex->pos >= 'A' && *lex->pos <= 'Z'))) {
      lex->token[1] = *lex->pos;
      lex->token[2] = '\0';
      lex->pos++;
      lex->token_type = 1;
      return 1;
    }

    /* Control word (letters) */
    size_t j = 1;
    while (*lex->pos && ((*lex->pos >= 'a' && *lex->pos <= 'z') ||
                         (*lex->pos >= 'A' && *lex->pos <= 'Z'))) {
      if (j < sizeof(lex->token) - 1) {
        lex->token[j++] = *lex->pos;
      }
      lex->pos++;
    }
    lex->token[j] = '\0';

    /* Eat trailing space after control word */
    if (*lex->pos == ' ') {
      lex->pos++;
    }

    lex->token_type = 1;
    return 1;
  }

  /* Regular character */
  size_t len = 1;
  /* Handle multi-byte UTF-8 */
  if ((c & 0xE0) == 0xC0) len = 2;
  else if ((c & 0xF0) == 0xE0) len = 3;
  else if ((c & 0xF8) == 0xF0) len = 4;

  memcpy(lex->token, lex->pos, len);
  lex->token[len] = '\0';
  lex->pos += len;
  lex->token_type = 0;
  return 0;
}
