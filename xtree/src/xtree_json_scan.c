#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>

#include "ds_strbuilder.h"

#include "xtree_reader.h"
#include "xtree_scan.h"
#include "xtree_json_scan.h"

static xtree_scan_token_t *scan_string (xtree_reader_t *reader)
{
  bool error = true;
  xtree_scan_token_t *ret = NULL;
  unsigned char *token = NULL;
  ds_strbuilder_t *sb = ds_strbuilder_new (NULL);

  // Record where we started reading this string.
  const char *filename = xtree_reader_name (reader);
  size_t line = xtree_reader_line (reader);
  size_t cpos = xtree_reader_cpos (reader);

  if (!sb)
    return NULL;

  // First make sure we weren't called in error.
  int c = xtree_reader_get (reader);
  if (c != '"')
    return NULL;

  while ((c = xtree_reader_get (reader)) != '"') {
    unsigned char tmp[2] = {(unsigned char)c, 0};

    // Unterminated string
    if (c == EOF) {
      ret = xtree_scan_token_new (xtree_json_token_ERROR,
                                  xtree_reader_name (reader),
                                  xtree_reader_line (reader),
                                  xtree_reader_cpos (reader),
                                  (const unsigned char *)"", 0);
      goto cleanup;
    }

    // Handle embedded newlines
    if (c == '\n') {
      if (!(ds_strbuilder_append (sb, "\\n")))
        goto cleanup;
    }
    if (c == '\r') {
      if (!(ds_strbuilder_append (sb, "\\r")))
        goto cleanup;
    }

    // Escaped characters are put in verbatim, unless a `\` is the last
    // character in the input (in which case it is probably malicious input).
    if (c == '\\') {
      tmp[0] = (unsigned char)c;
      if (!(ds_strbuilder_append (sb, (char *)tmp)))
        goto cleanup;
      c = xtree_reader_get (reader);
      if (c == EOF) {
        ret = xtree_scan_token_new (xtree_json_token_ERROR,
                                    xtree_reader_name (reader),
                                    xtree_reader_line (reader),
                                    xtree_reader_cpos (reader),
                                    (const unsigned char *)"", 0);
        goto cleanup;
      }
    }

    // The default is to append the character to the string we are building
    // up.
    tmp[0] = (unsigned char)c;
    if (!(ds_strbuilder_append (sb, (char *)tmp)))
      goto cleanup;
  }

  if (!(token = (unsigned char *)ds_strbuilder_build (sb)))
    goto cleanup;

  if (!(ret = xtree_scan_token_new (xtree_json_token_STRING,
                                    filename, line, cpos,
                                    token,
                                    strlen ((char *)token))))
    goto cleanup;

  error = false;
cleanup:
  ds_strbuilder_del (sb);
  if (error) {
    xtree_scan_token_free (&ret);
  }
  return ret;
}

static xtree_scan_token_t *scan_number (xtree_reader_t *reader)
{
  (void)reader;
  return NULL;
}



xtree_scan_token_t *xtree_json_scan (xtree_reader_t *reader)
{
  bool error = true;
  xtree_scan_token_t *ret = NULL;

  const char *filename = xtree_reader_name (reader);
  size_t line = xtree_reader_line (reader);
  size_t cpos = xtree_reader_cpos (reader);

  // Swallow all the whitespace
  int c = 0;
  while ((isspace (c = xtree_reader_peek (reader)))) {
    xtree_reader_get (reader);
  }

  /* *********************************************************************
    xtree_json_token_ERROR      = -1, // Error in scanner
    xtree_json_token_STRING,          // "..." (quotes retained, escapes as-is)
    xtree_json_token_NUMBER,          // -12.5e+3, .5, +1, 0x1F, 017, 0b101
    xtree_json_token_TRUE,            // true
    xtree_json_token_FALSE,           // false
    xtree_json_token_NULL,            // null
    xtree_json_token_COMMENT,         // // to end of line
    xtree_json_token_SYMBOL,          // any other unquoted word
    xtree_json_token_INFINITY,        // Infinity, +Infinity
    xtree_json_token_NEG_INFINITY,    // -Infinity
    xtree_json_token_NAN,             // NaN
    xtree_json_token_UNDEFINED        // undefined
   ********************************************************************* */
#define TOKEN_SCHAR(id,text) xtree_scan_token_new (id, filename, line, cpos, text, 1)

  switch (c) {

    // First the easy ones - return single character strings.
    case EOF:
      ret = TOKEN_SCHAR (xtree_json_token_EOF, (const unsigned char *)"");
      break;
    case '{':
      ret = TOKEN_SCHAR (xtree_json_token_LBRACE, (const unsigned char *)"{");
      break;
    case '}':
      ret = TOKEN_SCHAR (xtree_json_token_RBRACE, (const unsigned char *)"}");
      break;
    case '[':
      ret = TOKEN_SCHAR (xtree_json_token_LBRACKET, (const unsigned char *)"[");
      break;
    case ']':
      ret = TOKEN_SCHAR (xtree_json_token_RBRACKET, (const unsigned char *)"]");
      break;
    case ',':
      ret = TOKEN_SCHAR (xtree_json_token_COMMA, (const unsigned char *)",");
      break;
    case ':':
      ret = TOKEN_SCHAR (xtree_json_token_COLON, (const unsigned char *)":");
      break;

    // Next easy ones: tokens that are a single function call away
    case '"':
        ret = scan_string (reader);
        break;

    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        ret = scan_number (reader);
        break;
  }

#undef TOKEN_SCHAR

  error = false;
// cleanup:
  if (error) {
    xtree_scan_token_free (&ret);
  }
  return ret;
}

