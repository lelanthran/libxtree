#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "ds_str.h"
#include "ds_strbuilder.h"

#include "xtree_err.h"
#include "xtree_reader.h"
#include "xtree_scan.h"
#include "xtree_gtml_scan.h"


#define MIN(x,y)    (x < y ? x : y)

#define E(c,code,...)    do { \
  char *tmpmsg = NULL; \
  if (!(ds_str_printf (&tmpmsg, __VA_ARGS__))) { \
    ERROR (err, xtree_errcode_OOM, "[%s:%i] Failed to create errormsg", \
           __FILE__, __LINE__); \
  } else { \
    ERROR (err, xtree_errcode_UNEXPECTED_TOKEN, \
           "%s:%zu:%zu: near [%c]: %s", \
           filename, line, cpos, c, tmpmsg); \
  }\
  free (tmpmsg); \
} while (0);

static xtree_scan_token_t *scan_ecode (struct xtree_errobj_t *err,
                                       xtree_reader_t *reader, int c)
{
  bool error = true;
  xtree_scan_token_t *ret = NULL;
  unsigned char *token = NULL;
  unsigned char tmp[2] = { (unsigned char)c, 0 };
  ds_strbuilder_t *sb = ds_strbuilder_new (tmp, NULL);

  // Record where we started reading this string.
  const char *filename = xtree_reader_name (reader);
  size_t line = xtree_reader_line (reader);
  size_t cpos = xtree_reader_cpos (reader);

  if (!sb) {
    E (c, xtree_errcode_OOM, "Failed to construct a stringbuilder object.");
    goto cleanup;
  }

  c = xtree_reader_peek (reader);
  if (c != '#' && !(isalpha (c))) {
    E (c, xtree_errcode_UNEXPECTED_TOKEN, "Expected '#' or alpha character.");
    goto cleanup;
  }

  while ((c = xtree_reader_peek (reader)) &&
         (c != ';')) {
    tmp[0] = (unsigned char)xtree_reader_get (reader);
    if (!(ds_strbuilder_append (sb, tmp, NULL))) {
      E (c, xtree_errcode_OOM, "[%s]: Failed to append to strbuilder.", tmp);
      goto cleanup;
    }
  }

  if ((c = xtree_reader_get (reader)) != ';') {
    E (c, xtree_errcode_UNTERMINATED_ECODE, "Escape code missing terminator.");
    goto cleanup;
  }
  if (!(ds_strbuilder_append (sb, (unsigned char *)";", NULL))) {
    E (c, xtree_errcode_OOM, "[;]: Failed to append to strbuilder.");
    goto cleanup;
  }

  if (!(token = (unsigned char *)ds_strbuilder_build (sb))) {
    E (c, xtree_errcode_OOM, "Failed to build string.");
    goto cleanup;
  }

  if (!(ret = xtree_scan_token_new (xtree_gtml_token_ECODE,
                                    filename, line, cpos,
                                    token,
                                    strlen ((char *)token)))) {
    E (c, xtree_errcode_OOM, "[%s]: Failed to create token.", token);
    goto cleanup;
  }

  error = false;
cleanup:
  free (token);
  ds_strbuilder_del (sb);
  if (error) {
    xtree_scan_token_free (&ret);
    ret = NULL;
  }
  return ret;
}

static xtree_scan_token_t *scan_ws (struct xtree_errobj_t *err,
                                    xtree_reader_t *reader, int c)
{
  bool error = true;
  xtree_scan_token_t *ret = NULL;
  unsigned char *token = NULL;
  unsigned char tmp[2] = { (unsigned char)c, 0 };
  ds_strbuilder_t *sb = ds_strbuilder_new (tmp, NULL);

  // Record where we started reading this string.
  const char *filename = xtree_reader_name (reader);
  size_t line = xtree_reader_line (reader);
  size_t cpos = xtree_reader_cpos (reader);

  if (!sb) {
    E (c, xtree_errcode_OOM, "Failed to construct a stringbuilder object.");
    goto cleanup;
  }

  while ((c = xtree_reader_peek (reader)) &&
         (isspace (c))) {
    tmp[0] = (unsigned char)xtree_reader_get (reader);
    if (!(ds_strbuilder_append (sb, tmp, NULL))) {
      E (c, xtree_errcode_OOM, "[%s]: Failed to append to strbuilder.", tmp);
      goto cleanup;
    }
  }

  if (!(token = (unsigned char *)ds_strbuilder_build (sb))) {
    E (c, xtree_errcode_OOM, "Failed to build string.");
    goto cleanup;
  }

  if (!(ret = xtree_scan_token_new (xtree_gtml_token_WSPACE,
                                    filename, line, cpos,
                                    token,
                                    strlen ((char *)token)))) {
    E (c, xtree_errcode_OOM, "[%s]: Failed to create token.", token);
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
}

static xtree_scan_token_t *scan_string (struct xtree_errobj_t *err,
                                        xtree_reader_t *reader, int c)
{
  /* String parsing is really simple - anything inside quotes is collected into
   * a string token. Some processing may be required inside that string:
   * - Pass escaped characters unchanged (no processesing).
   * - Ensure the escape character is followed by *something*; an input with
   *   the escape as the final character in the string is probably malicious.
   */
  bool error = true;
  xtree_scan_token_t *ret = NULL;
  unsigned char *token = NULL;
  unsigned char tmp[2] = {(unsigned char)c, 0};
  ds_strbuilder_t *sb = ds_strbuilder_new (tmp, NULL);

  // Record where we started reading this string.
  const char *filename = xtree_reader_name (reader);
  size_t line = xtree_reader_line (reader);
  size_t cpos = xtree_reader_cpos (reader);

  if (!sb) {
    E (c, xtree_errcode_OOM, "Failed to construct a stringbuilder object.");
    goto cleanup;
  }

  int qchar = c;

  while ((c = xtree_reader_peek (reader)) &&
         (c != EOF && c > 0) &&
         (c != qchar)) {
    c = xtree_reader_get (reader);
    tmp[0] = (unsigned char)c;

    // Escaped characters are put in verbatim, unless a `\` is the last
    // character in the input (in which case it is probably malicious input).
    if (c == '\\') {
      tmp[0] = (unsigned char)c;
      if (!(ds_strbuilder_append (sb, tmp, NULL))) {
        E (c, xtree_errcode_OOM, "[%s]: Failed to append to strbuilder.", tmp);
        goto cleanup;
      }
      c = xtree_reader_get (reader);
      if (c == EOF || c == 0) {
        break;
      }
    }

    // The default is to append the character to the string we are building
    // up.
    tmp[0] = (unsigned char)c;
    if (!(ds_strbuilder_append (sb, tmp, NULL))) {
      E (c, xtree_errcode_OOM, "[%s]: Failed to append to strbuilder.", tmp);
      goto cleanup;
    }
  }
  if (c == EOF || c == 0) {
    ret = xtree_scan_token_new (xtree_gtml_token_ERROR,
                                xtree_reader_name (reader),
                                xtree_reader_line (reader),
                                xtree_reader_cpos (reader),
                                (const unsigned char *)"", 0);
    E (c, xtree_errcode_UNTERMINATED_STRING, "Unterminated string.");
    error = false;
    goto cleanup;
  }
  tmp[0] = (unsigned char)xtree_reader_get (reader);

  if (!(ds_strbuilder_append (sb, tmp, NULL))) {
    E (c, xtree_errcode_OOM, "[%s]: Failed to append to strbuilder.", tmp);
    goto cleanup;
  }

  if (!(token = (unsigned char *)ds_strbuilder_build (sb))) {
    E (c, xtree_errcode_OOM, "Failed to build string.");
    goto cleanup;
  }

  if (!(ret = xtree_scan_token_new (xtree_gtml_token_STRING,
                                    filename, line, cpos,
                                    token,
                                    strlen ((char *)token)))) {
    E (c, xtree_errcode_OOM, "[%s]: Failed to create token.", token);
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
}


static xtree_scan_token_t *scan_symbol (struct xtree_errobj_t *err,
                                        xtree_reader_t *reader, int c)
{
  /* Symbols are everything up to a set of delimiters (whitespace, comma,
   * quote, etc).
   */
  static const char *punctuation =
    "<=/&'\">?![]"
    "";

  bool error = true;
  xtree_scan_token_t *ret = NULL;
  unsigned char *token = NULL;
  unsigned char tmp[2] = { (unsigned char )c, 0 };
  ds_strbuilder_t *sb = ds_strbuilder_new (tmp, NULL);

  // Record where we started reading this string.
  const char *filename = xtree_reader_name (reader);
  size_t line = xtree_reader_line (reader);
  size_t cpos = xtree_reader_cpos (reader);

  if (!sb) {
    E (c, xtree_errcode_OOM, "Failed to construct a stringbuilder object.");
    goto cleanup;
  }

  while ((c = xtree_reader_peek (reader)) &&
         ((strchr (punctuation, c)) == NULL) &&
         (!(isspace (c)))) {
    tmp[0] = (unsigned char)xtree_reader_get (reader);
    if (!(ds_strbuilder_append (sb, tmp, NULL))) {
      E (c, xtree_errcode_OOM, "[%s]: Failed to append to strbuilder.", tmp);
      goto cleanup;
    }
  }

  if (!(token = (unsigned char *)ds_strbuilder_build (sb))) {
    E (c, xtree_errcode_OOM, "Failed to build string.");
    goto cleanup;
  }

  if (!(ret = xtree_scan_token_new (xtree_gtml_token_SYMBOL,
                                    filename, line, cpos,
                                    token,
                                    strlen ((char *)token)))) {
    E (c, xtree_errcode_OOM, "[%s]: Failed to create token.", token);
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
}


static xtree_scan_token_t *scan_cdata (struct xtree_errobj_t *err,
                                       xtree_reader_t *reader,
                                       int c)
{
  bool error = true;
  xtree_scan_token_t *ret = NULL;
  unsigned char *token = NULL;

  // Record where we started reading the CDATA.
  const char *filename = xtree_reader_name (reader);
  size_t line = xtree_reader_line (reader);
  size_t cpos = xtree_reader_cpos (reader);

  // Swallow the remaining tag characters
  static const char start[] = "<![CDATA[";
  for (size_t i=0; i < (sizeof start) - 1; i++) {
    c = xtree_reader_get (reader);
  }
  unsigned char tmp[2] = { (unsigned char )c, 0 };
  ds_strbuilder_t *sb = ds_strbuilder_new (tmp, NULL);

  if (!sb) {
    E (c, xtree_errcode_OOM, "Failed to construct a stringbuilder object.");
    goto cleanup;
  }

  static const unsigned char delim[] = "]]>";
  unsigned char lookahead[(sizeof delim) + 1] = "";

  while ((c = xtree_reader_peek (reader)) &&
         ((strcmp ((char *)delim, (char *)lookahead)) != 0)) {
    tmp[0] = (unsigned char)xtree_reader_get (reader);
    if (!(ds_strbuilder_append (sb, tmp, NULL))) {
      E (c, xtree_errcode_OOM, "[%s]: Failed to append to strbuilder.", tmp);
      goto cleanup;
    }
    xtree_reader_lookahead (reader, lookahead, sizeof lookahead);
  }

  if ((strcmp ((char *)delim, (char *)lookahead)) != 0) {
    E (c, xtree_errcode_OOM, "[%s]: Unterminated CDATA section.", tmp);
    goto cleanup;
  }

  // Swallow the remaining end tag characters
  for (size_t i=0; i < (sizeof delim) - 1; i++) {
    c = xtree_reader_get (reader);
  }
  if (!(token = (unsigned char *)ds_strbuilder_build (sb))) {
    E (c, xtree_errcode_OOM, "Failed to build string.");
    goto cleanup;
  }

  if (!(ret = xtree_scan_token_new (xtree_gtml_token_CDATA,
                                    filename, line, cpos,
                                    token,
                                    strlen ((char *)token)))) {
    E (c, xtree_errcode_OOM, "[%s]: Failed to create token.", token);
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
}

xtree_scan_token_t *xtree_gtml_scan (struct xtree_errobj_t *err,
                                     xtree_reader_t *reader)
{
  xtree_scan_token_t *ret = NULL;

  int c = 0;

  const char *filename = xtree_reader_name (reader);
  size_t line = xtree_reader_line (reader);
  size_t cpos = xtree_reader_cpos (reader);

  xtree_errobj_reset (err, 1024);

  c = xtree_reader_get (reader);
  unsigned char tmp[2] = { (unsigned char)c, 0 };

#define TOKEN_SCHAR(id) do { \
  ret = xtree_scan_token_new (id, filename, line, cpos, tmp, 1); \
  if (!ret) { \
    E (c, xtree_errcode_OOM, "[%s]: Failed to construct token", tmp); \
    goto cleanup; \
  } \
} while (0);

  // Whitespace check is handled before any other check, because a function is
  // used to determine if a space is encountered
  if ((isspace (c))) {
    ret = scan_ws (err, reader, c);
    goto cleanup;
  }

  static const unsigned char cdata[] = "![CDATA[";
  unsigned char lookahead[(sizeof cdata) + 1];

  switch (c) {

    // First the easiest ones - return single character strings.
    case 0:
    case EOF: TOKEN_SCHAR (xtree_gtml_token_EOF);       break;
    case '>': TOKEN_SCHAR (xtree_gtml_token_GT);        break;
    case '=': TOKEN_SCHAR (xtree_gtml_token_EQUAL);     break;
    case '/': TOKEN_SCHAR (xtree_gtml_token_SLASH);     break;
    case '!': TOKEN_SCHAR (xtree_gtml_token_EXCL);      break;
    case '#': TOKEN_SCHAR (xtree_gtml_token_HASH);      break;
    case '?': TOKEN_SCHAR (xtree_gtml_token_QMARK);     break;
    case '[': TOKEN_SCHAR (xtree_gtml_token_LBRACKET);  break;
    case ']': TOKEN_SCHAR (xtree_gtml_token_RBRACKET);  break;

    // Next easy ones: tokens that are a single function call away: escape codes
    // and strings
    case '&': ret = scan_ecode (err, reader, c);     break;
    case '"':
    case '\'': ret = scan_string (err, reader, c);   break;

    // Finally, the more complex ones (contain a conditional)
    case '<':
      xtree_reader_lookahead (reader, lookahead, sizeof lookahead);
#if 0
      printf ("******** %zu [%s]  %zu [%s]\n",
              sizeof cdata, cdata,
              sizeof lookahead, lookahead);
#endif
      if ((strncmp ((char *)lookahead, (char *)cdata, sizeof cdata)) == 0) {
        ret = scan_cdata (err, reader, c);
      } else {
        TOKEN_SCHAR (xtree_gtml_token_LT);
      }
      break;

    default:
        ret = scan_symbol (err, reader, c);
        break;
  }

#undef TOKEN_SCHAR

cleanup:
  if (xtree_errchk (err)) {
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
    X (xtree_gtml_token_SENTINEL),
    X (xtree_gtml_token_UNRECOGNISED),
    X (xtree_gtml_token_ERROR),
    X (xtree_gtml_token_EOF),
    X (xtree_gtml_token_LT),
    X (xtree_gtml_token_GT),
    X (xtree_gtml_token_EQUAL),
    X (xtree_gtml_token_SLASH),
    X (xtree_gtml_token_EXCL),
    X (xtree_gtml_token_HASH),
    X (xtree_gtml_token_QMARK),
    X (xtree_gtml_token_LBRACKET),
    X (xtree_gtml_token_RBRACKET),
    X (xtree_gtml_token_WSPACE),
    X (xtree_gtml_token_STRING),
    X (xtree_gtml_token_ECODE),
    X (xtree_gtml_token_CDATA),
    X (xtree_gtml_token_SYMBOL),
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
