#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "ds_strbuilder.h"

#include "xtree_err.h"
#include "xtree_reader.h"
#include "xtree_scan.h"
#include "xtree_gtml_scan.h"


#define MIN(x,y)    (x < y ? x : y)


static xtree_scan_token_t *scan_string (xtree_reader_t *reader)
{
  /* String parsing is really simple - anything inside double-quotes is
   * collected into a string token. Some processing may be required inside
   * that string:
   * - Replace literal newlines and carriage returns with escaped newlines
   *   and carriage returns..
   * - Pass escaped characters unchanged (no processesing).
   * - Ensure the escape character is followed by *something*; an input with
   *   the escape as the final character in the string is probably malicious.
   */
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

  // First, swallow the starting quote, and make sure we weren't called in
  // error.
  int c = xtree_reader_get (reader);
  if (c != '"')
    return NULL;

  while ((c = xtree_reader_get (reader)) != '"') {
    unsigned char tmp[2] = {(unsigned char)c, 0};

    // Unterminated string
    if (c == EOF || c == 0) {
      break;
    }

    // Handle embedded newlines
    if (c == '\n') {
      if (!(ds_strbuilder_append (sb, (const unsigned char *)"\\n", NULL)))
        goto cleanup;
      continue;
    }
    if (c == '\r') {
      if (!(ds_strbuilder_append (sb, (const unsigned char *)"\\r", NULL)))
        goto cleanup;
      continue;
    }

    // Escaped characters are put in verbatim, unless a `\` is the last
    // character in the input (in which case it is probably malicious input).
    if (c == '\\') {
      tmp[0] = (unsigned char)c;
      if (!(ds_strbuilder_append (sb, tmp, NULL)))
        goto cleanup;
      c = xtree_reader_get (reader);
      if (c == EOF || c == 0) {
        break;
      }
    }

    // The default is to append the character to the string we are building
    // up.
    tmp[0] = (unsigned char)c;
    if (!(ds_strbuilder_append (sb, tmp, NULL)))
      goto cleanup;
  }
  if (c == EOF || c == 0) {
    ret = xtree_scan_token_new (xtree_gtml_token_ERROR,
                                xtree_reader_name (reader),
                                xtree_reader_line (reader),
                                xtree_reader_cpos (reader),
                                (const unsigned char *)"", 0);
    error = false;
    goto cleanup;
  }

  if (!(token = (unsigned char *)ds_strbuilder_build (sb)))
    goto cleanup;

  if (!(ret = xtree_scan_token_new (xtree_gtml_token_STRING,
                                    filename, line, cpos,
                                    token,
                                    strlen ((char *)token))))
    goto cleanup;

  error = false;
cleanup:
  free (token);
  ds_strbuilder_del (sb);
  if (error) {
    xtree_scan_token_free (&ret);
  }
  return ret;
}



static xtree_scan_token_t *scan_allowed (xtree_reader_t *reader,
                                         enum xtree_gtml_token_id_t id,
                                         const char *allowed)
{
  bool error = true;
  xtree_scan_token_t *ret = NULL;
  ds_strbuilder_t *sb = ds_strbuilder_new (NULL);
  unsigned char *token = NULL;

  const char *filename = xtree_reader_name (reader);
  size_t line = xtree_reader_line (reader);
  size_t cpos = xtree_reader_cpos (reader);

  unsigned char tmp[2] = {0, 0};
  int c = 0;
  while ((c = xtree_reader_peek (reader)) && strchr (allowed, c)) {
    tmp[0] = (unsigned char)xtree_reader_get (reader);
    if (!(ds_strbuilder_append (sb, tmp, NULL))) {
      goto cleanup;
    }
  }

  if (!(token = (unsigned char *)ds_strbuilder_build (sb)))
    goto cleanup;

  if (!(ret = xtree_scan_token_new (id,
                                    filename, line, cpos,
                                    token,
                                    strlen ((char *)token))))
    goto cleanup;

  error = false;
cleanup:
  free (token);
  ds_strbuilder_del (sb);
  if (error) {
    xtree_scan_token_free (&ret);
  }
  return ret;
}



static xtree_scan_token_t *scan_disallowed (xtree_reader_t *reader,
                                            enum xtree_gtml_token_id_t id,
                                            const char *allowed)
{
  bool error = true;
  xtree_scan_token_t *ret = NULL;
  ds_strbuilder_t *sb = ds_strbuilder_new (NULL);
  unsigned char *token = NULL;

  const char *filename = xtree_reader_name (reader);
  size_t line = xtree_reader_line (reader);
  size_t cpos = xtree_reader_cpos (reader);

  unsigned char tmp[2] = {0, 0};
  int c = 0;
  while ((c = xtree_reader_peek (reader)) &&
          c != EOF &&
          !(isspace (c)) &&
          !(strchr (allowed, c))) {
    tmp[0] = (unsigned char)xtree_reader_get (reader);
    if (!(ds_strbuilder_append (sb, tmp, NULL))) {
      goto cleanup;
    }
  }

  if (!(token = (unsigned char *)ds_strbuilder_build (sb)))
    goto cleanup;

  if (!(ret = xtree_scan_token_new (id,
                                    filename, line, cpos,
                                    token,
                                    strlen ((char *)token))))
    goto cleanup;

  error = false;
cleanup:
  free (token);
  ds_strbuilder_del (sb);
  if (error) {
    xtree_scan_token_free (&ret);
  }
  return ret;
}



static xtree_scan_token_t *scan_number (xtree_reader_t *reader)
{
  /* Numbers are more difficult to parse than one would think; there are
   * multiple paths:
   * - Hex numbers must start with `0x` or `0X`.
   * - Binary numbers must start with `0b` or `0B`.
   * - Octal numbers must start with a `0` with no `x`/`X` or no `b`/`B`as the
   *   second character
   * - All other numbers
   * Additionally, that final path (all other numbers) must be parsed as
   * follows:
   * - Read the optional sign
   * - Read a number of mandatory digits
   * - If no decimal point is found after all the digits, end processing
   * - If a decimal point is found, read a number of optional digits
   * - etc, etc, etc
   *
   * We can resolve this gordians knot by simply not doing the parser step
   * during this scanning step! Anything that starts with a number-character
   * (such as the comma, period, sign or digit) is scanned as a number token
   * until a disallowed character is found, and we let the parser step later
   * fall over and throw an error if the number token is malformed.
   */
  static const char *allowed =
    ".-+/"
    "0123456789"
    "abcdef"
    "ABCDEF"
    "eExXbB"
    "";

  static const struct {
    enum xtree_gtml_token_id_t id;
    const char *text;
  } exemptions[] = {
    { xtree_gtml_token_INFINITY,      "+Infinity"  },
    { xtree_gtml_token_NEG_INFINITY,  "-Infinity"  },
    { xtree_gtml_token_NAN,           "+NaN"       },
    { xtree_gtml_token_NEG_NAN,       "-NaN"       },
    { xtree_gtml_token_NULL,          "-null"      },
    { xtree_gtml_token_NULL,          "+null"      },
  };
  static size_t nexemptions = sizeof exemptions / sizeof exemptions[0];

  xtree_scan_token_t *ret = scan_allowed (reader, xtree_gtml_token_NUMBER, allowed);

  for (size_t i=0; i<nexemptions; i++) {
    if ((strcmp (exemptions[i].text, (const char *)xtree_scan_token_text (ret))) == 0) {
      xtree_scan_token_id_set (ret, exemptions[i].id);
      break;
    }
  }

  return ret;
}


static xtree_scan_token_t *scan_comment (xtree_reader_t *reader)
{
  bool error = true;
  xtree_scan_token_t *ret = NULL;
  ds_strbuilder_t *sb = ds_strbuilder_new ((unsigned char *)"//", NULL);
  unsigned char *token = NULL;

  const char *filename = xtree_reader_name (reader);
  size_t line = xtree_reader_line (reader);
  size_t cpos = xtree_reader_cpos (reader);

  int c1 = xtree_reader_get (reader);
  int c2 = xtree_reader_get (reader);
  if (c1 != '/' || c2 !='/') {
    ret = xtree_scan_token_new (xtree_gtml_token_ERROR,
                                filename, line, cpos,
                                (const unsigned char *)"", 0);
    error = false;
    goto cleanup;
  }

  unsigned char tmp[2] = {0, 0};
  while ((c1 = xtree_reader_peek (reader)) && c1 != '\n' && c1 != EOF) {
    if (c1 == '\r') {
      xtree_reader_get (reader);
      continue;
    }
    tmp[0] = (unsigned char)xtree_reader_get (reader);
    if (!(ds_strbuilder_append (sb, tmp, NULL))) {
      goto cleanup;
    }
  }

  if (!(token = (unsigned char *)ds_strbuilder_build (sb)))
    goto cleanup;

  if (!(ret = xtree_scan_token_new (xtree_gtml_token_COMMENT,
                                    filename, line, cpos,
                                    token, strlen ((const char *)token)))) {
    goto cleanup;
  }
  error = false;
cleanup:
  free (token);
  ds_strbuilder_del (sb);
  if (error) {
    xtree_scan_token_free (&ret);
  }
  return ret;
  return NULL;
}



static xtree_scan_token_t *scan_symbol (xtree_reader_t *reader)
{
  /* Symbols are everything up to a set of delimiters (whitespace, comma,
   * etc). Some symbols are special: `true`, `false` and `null`. These are
   * handled by changing just the `type` field of the return value.
   */
  static const char *allowed =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789"
    // *Very* generous interpretation for "symbol"; allow things like $(FOO),
    // <tagname>, name=value, etc. `{}` are not allowed (conflict with
    // JSON) and `[]` are not allowed (also conflicts with JSON), and commas
    // are not allowed (also conflicts with valid JSON).
    "!@#$%^&*()_+=-;><?.'\\";
  static const char *disallowed =
    // Copy of "allowed"
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789"
    "!@#$%^&*()_+=-;><?.'\\"
    // Additions to "allowed"
    ",[]{}:\"";

  static const struct {
    enum xtree_gtml_token_id_t id;
    const char *text;
  } exemptions[] = {
    { xtree_gtml_token_TRUE,      "true"      },
    { xtree_gtml_token_FALSE,     "false"     },
    { xtree_gtml_token_NULL,      "null"      },
    { xtree_gtml_token_INFINITY,  "Infinity"  },
    { xtree_gtml_token_NAN,       "NaN"       },
    { xtree_gtml_token_UNDEFINED, "undefined" },
  };
  static size_t nexemptions = sizeof exemptions / sizeof exemptions[0];

  xtree_scan_token_t *ret = scan_allowed (reader, xtree_gtml_token_SYMBOL, allowed);

  for (size_t i=0; ret && i < nexemptions; i++) {
    if ((strcmp (exemptions[i].text, (const char *)xtree_scan_token_text (ret))) == 0) {
      xtree_scan_token_id_set (ret, exemptions[i].id);
      break;
    }
  }
  if (ret && (strlen ((char *)xtree_scan_token_text (ret)) == 0)) {
    xtree_scan_token_free (&ret);
    ret = scan_disallowed (reader, xtree_gtml_token_UNRECOGNISED, disallowed);
  }
  return ret;
}



xtree_scan_token_t *xtree_gtml_scan (xtree_reader_t *reader)
{
  bool error = true;
  xtree_scan_token_t *ret = NULL;

  // Swallow all the whitespace
  int c = 0;
  while ((isspace (c = xtree_reader_peek (reader)))) {
    xtree_reader_get (reader);
  }

  const char *filename = xtree_reader_name (reader);
  size_t line = xtree_reader_line (reader);
  size_t cpos = xtree_reader_cpos (reader);

  // Handle the comment, as that is a special case (prefixed with 2x
  // characters, not one).
  if (c == '/') {
    int tmpc = xtree_reader_get (reader);
    if (tmpc == xtree_reader_peek (reader)) {
      xtree_reader_unget (reader);
      return scan_comment (reader);
    }
    xtree_reader_unget (reader);
  }

#define TOKEN_SCHAR(id,text) (xtree_reader_get (reader), xtree_scan_token_new (id, filename, line, cpos, text, 1))

  switch (c) {

    // First the easiest ones - return single character strings.
    case 0:
    case EOF:
      ret = TOKEN_SCHAR (xtree_gtml_token_EOF, (const unsigned char *)"");
      break;
    case '{':
      ret = TOKEN_SCHAR (xtree_gtml_token_LBRACE, (const unsigned char *)"{");
      break;
    case '}':
      ret = TOKEN_SCHAR (xtree_gtml_token_RBRACE, (const unsigned char *)"}");
      break;
    case '[':
      ret = TOKEN_SCHAR (xtree_gtml_token_LBRACKET, (const unsigned char *)"[");
      break;
    case ']':
      ret = TOKEN_SCHAR (xtree_gtml_token_RBRACKET, (const unsigned char *)"]");
      break;
    case ',':
      ret = TOKEN_SCHAR (xtree_gtml_token_COMMA, (const unsigned char *)",");
      break;
    case ':':
      ret = TOKEN_SCHAR (xtree_gtml_token_COLON, (const unsigned char *)":");
      break;

    // Next easy ones: tokens that are a single function call away (numbers,
    // strings and symbols
    case '"':
        ret = scan_string (reader);
        break;

    case '-':
    case '+':
    case '.':
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

    default:
        ret = scan_symbol (reader);
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

const char *xtree_gtml_token_id_str (enum xtree_gtml_token_id_t id)
{
  static const struct {
    enum xtree_gtml_token_id_t id;
    const char *str;
  } ids[] = {
#define X(id) { id, #id }
    X (xtree_gtml_token_RESERVED),
    X (xtree_gtml_token_ERROR),
    X (xtree_gtml_token_EOF),
    X (xtree_gtml_token_LBRACE),
    X (xtree_gtml_token_RBRACE),
    X (xtree_gtml_token_LBRACKET),
    X (xtree_gtml_token_RBRACKET),
    X (xtree_gtml_token_COLON),
    X (xtree_gtml_token_COMMA),
    X (xtree_gtml_token_STRING),
    X (xtree_gtml_token_NUMBER),
    X (xtree_gtml_token_TRUE),
    X (xtree_gtml_token_FALSE),
    X (xtree_gtml_token_NULL),
    X (xtree_gtml_token_COMMENT),
    X (xtree_gtml_token_SYMBOL),
    X (xtree_gtml_token_INFINITY),
    X (xtree_gtml_token_NEG_INFINITY),
    X (xtree_gtml_token_NAN),
    X (xtree_gtml_token_NEG_NAN),
    X (xtree_gtml_token_UNDEFINED),
    X (xtree_gtml_token_UNRECOGNISED),
  };
#undef X
  static char unknown[] = "XXXXXXXXXXXXXXX - Unknown JSON token id";

  for (size_t i=0; i<sizeof ids / sizeof ids[0]; i++) {
    if (ids[i].id == id) {
      return ids[i].str;
    }
  }
  snprintf (unknown, sizeof unknown, "%i - Unknown JSON token id", id);
  return unknown;
}
