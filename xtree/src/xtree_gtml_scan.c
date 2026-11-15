#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <inttypes.h>

#include "ds_str.h"
#include "ds_strbuilder.h"

#include "xtree_err.h"
#include "xtree_reader.h"
#include "xtree_scan.h"
#include "xtree_gtml_scan.h"


#define MIN(x,y)    (x < y ? x : y)

#define E(reader,code,...)    do { \
  char *tmpmsg = NULL; \
  if (!(ds_str_printf (&tmpmsg, __VA_ARGS__))) { \
    ERROR (err, xtree_errcode_OOM, "[%s:%i] Failed to create errormsg", \
           __FILE__, __LINE__); \
  } else { \
    static unsigned char la[20]; \
    xtree_reader_lookahead (reader, la, sizeof la); \
    ERROR (err, xtree_errcode_UNEXPECTED_TOKEN, \
           "%s:%zu:%zu: near `%s`: %s", \
           xtree_reader_name (reader), \
           xtree_reader_line (reader), \
           xtree_reader_cpos (reader), \
           la, \
           tmpmsg); \
  }\
  free (tmpmsg); \
} while (0);


#define U(s)          ((unsigned char *)s)
#define C(s)          ((char *)s)

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

    X (xtree_gtml_token_WSPACE),
    X (xtree_gtml_token_EQUAL),
    X (xtree_gtml_token_GT),
    X (xtree_gtml_token_SCLOSING),
    X (xtree_gtml_token_CDATA),
    X (xtree_gtml_token_TAGNAME),
    X (xtree_gtml_token_CLOSE_TAG),
    X (xtree_gtml_token_ATTR_NAME),
    X (xtree_gtml_token_ATTR_VALUE),
    X (xtree_gtml_token_CONTENT),
    X (xtree_gtml_token_ECODE),
    X (xtree_gtml_token_COMMENT),
    X (xtree_gtml_token_DIRECTIVE),
  };
#undef X
  static char unknown[] = "XXXXXXXXXXXXXXX - Unknown GTML token id";

  for (size_t i=0; i<sizeof ids / sizeof ids[0]; i++) {
    if (ids[i].id == id) {
      return ids[i].str;
    }
  }
  snprintf (unknown, sizeof unknown, "%i - Unknown GTML token id", id);
  return unknown;
}




// Consume the begin marker
#define F_CONSUME_BEGIN   ((uint64_t)1 << 0)
// Consume the end marker
#define F_CONSUME_END     ((uint64_t)1 << 1)
// Consume bo5th begin and end markers
#define F_CONSUME_BOTH    ((uint64_t)(F_CONSUME_BEGIN | F_CONSUME_END))
// Consume leading spaces (but not trailing ones)
#define F_CONSUME_WSPACE  ((uint64_t)1 << 2)
// Don't mark EOF as an error
#define F_ALLOW_EOF       ((uint64_t)1 << 3)


xtree_scan_token_t *collect_between (struct xtree_errobj_t *err,
                                     xtree_reader_t *reader,
                                     int id,
                                     uint64_t flags,
                                     const unsigned char *begin,
                                     const unsigned char *end)
{
  xtree_scan_token_t *ret = NULL;

  ds_strbuilder_t *sb = NULL;
  unsigned char *lookahead = NULL;
  unsigned char tmp[2] = { 0,  0 };
  unsigned char *text = NULL;
  int c = 0;

  size_t begin_len = strlen (C(begin));
  size_t end_len = strlen (C(end));
  size_t max_len = begin_len > end_len ? begin_len : end_len;

  if (!(lookahead = calloc (1, max_len + 1))) {
    ERROR (err, xtree_errcode_OOM,
           "OOM allocating lookahead buffer of %zu bytes", max_len);
    goto cleanup;
  }

  if (!(sb = ds_strbuilder_new (NULL))) {
    ERROR (err, xtree_errcode_OOM, "OOM allocating string builder");
    goto cleanup;
  }

  xtree_reader_lookahead (reader, lookahead, begin_len + 1);
  if ((strncmp (C(lookahead), C(begin), begin_len)) != 0) {
    E (reader, xtree_errcode_UNEXPECTED_TOKEN,
               "section must start with `%s`", begin);
    goto cleanup;
  }

  // The begin string matches, swallow it
  for (size_t i=0; (flags & F_CONSUME_BEGIN) && (i < begin_len); i++) {
    xtree_reader_get (reader);
  }

  while ((xtree_reader_lookahead (reader, lookahead, end_len + 1)) &&
         (strncmp (C(lookahead), C(end), end_len) != 0) &&
         (c = xtree_reader_peek (reader)) &&
         (c > 0)) {
    tmp[0] = (unsigned char)xtree_reader_get (reader);
    if (!(ds_strbuilder_append (sb, tmp, NULL))) {
      ERROR (err, xtree_errcode_OOM, "OOM appending to string builder");
      goto cleanup;
    }
  }

  if (c < 0) {
    E (reader, xtree_errcode_UNEXPECTED_TOKEN, "Unexpected EOF");
    goto cleanup;
  }

  if ((strncmp (C(lookahead), C(end), end_len)) != 0) {
    E (reader, xtree_errcode_UNEXPECTED_TOKEN, "Unexpected token found");
    goto cleanup;
  }

  // Consume end chars
  for (size_t i=0; (flags & F_CONSUME_END) && (i < end_len); i++) {
    xtree_reader_get (reader);
  }

  if (!(text = U(ds_strbuilder_build (sb)))) {
    ERROR (err, xtree_errcode_OOM, "String builder construction failure");
    goto cleanup;
  }

  if (!(ret = xtree_scan_token_new (id,
                                    xtree_reader_name (reader),
                                    xtree_reader_line (reader),
                                    xtree_reader_cpos (reader),
                                    text, strlen (C(text))))) {
    ERROR (err, xtree_errcode_OOM,
           "Failed to construct token %i [%s]", id, text);
    goto cleanup;
  }

cleanup:
  free (lookahead);
  free (text);
  ds_strbuilder_del (sb);
  if (xtree_errchk (err)) {
    xtree_scan_token_free (&ret);
  }
  return ret;
}

xtree_scan_token_t *collect_until (struct xtree_errobj_t *err,
                                   xtree_reader_t *reader,
                                   int id,
                                   uint64_t flags,
                                   const unsigned char *begin,
                                   const unsigned char *delims)
{
  xtree_scan_token_t *ret = NULL;

  ds_strbuilder_t *sb = NULL;
  unsigned char *lookahead = NULL;
  unsigned char tmp[2] = { 0,  0 };
  unsigned char *text = NULL;
  int c = 0;

  size_t begin_len = strlen (C(begin)) + 1;

  while ((flags & F_CONSUME_WSPACE) &&
         (isspace(xtree_reader_peek (reader)))) {
    xtree_reader_get (reader);
  }

  if (!(lookahead = calloc (1, begin_len + 1))) {
    ERROR (err, xtree_errcode_OOM,
           "OOM allocating lookahead buffer of %zu bytes", begin_len);
    goto cleanup;
  }

  if (!(sb = ds_strbuilder_new (NULL))) {
    ERROR (err, xtree_errcode_OOM, "OOM allocating string builder");
    goto cleanup;
  }

  xtree_reader_lookahead (reader, lookahead, begin_len);
  if (begin[0] && (strcmp (C(begin), C(lookahead))) != 0) {
    E (reader, xtree_errcode_UNEXPECTED_TOKEN,
               "section must start with `%s`", begin);
    goto cleanup;
  }

  // The begin string matches, swallow it
  for (size_t i=0; (flags & F_CONSUME_BEGIN) && (i < (begin_len - 1)); i++) {
    xtree_reader_get (reader);
  }

  while ((c = xtree_reader_peek (reader)) &&
         (c > 0) &&
         ((strchr (C(delims), c) == NULL))) {
    tmp[0] = (unsigned char)xtree_reader_get (reader);
    if (!(ds_strbuilder_append (sb, tmp, NULL))) {
      ERROR (err, xtree_errcode_OOM, "OOM appending to string builder");
      goto cleanup;
    }
  }

  if ((!(flags & F_ALLOW_EOF)) && (c <= 0)) {
    E (reader, xtree_errcode_UNEXPECTED_TOKEN, "Unexpected EOF");
    goto cleanup;
  }

  if ((!(flags & F_ALLOW_EOF)) && (strchr (C(delims), c)) == NULL) {
    E (reader, xtree_errcode_UNEXPECTED_TOKEN, "Unexpected token found");
    goto cleanup;
  }

  // Consume any sequence of end delimiters
  while ((flags & F_CONSUME_END) &&
         (c = xtree_reader_peek (reader)) &&
         (strchr (C(delims), c))) {
    xtree_reader_get (reader);
  }

  if (!(text = U(ds_strbuilder_build (sb)))) {
    ERROR (err, xtree_errcode_OOM, "String builder construction failure");
    goto cleanup;
  }

  if (!(ret = xtree_scan_token_new (id,
                                    xtree_reader_name (reader),
                                    xtree_reader_line (reader),
                                    xtree_reader_cpos (reader),
                                    text, strlen (C(text))))) {
    ERROR (err, xtree_errcode_OOM,
           "Failed to construct token %i [%s]", id, text);
    goto cleanup;
  }

cleanup:
  free (lookahead);
  free (text);
  ds_strbuilder_del (sb);
  if (xtree_errchk (err)) {
    xtree_scan_token_free (&ret);
  }
  return ret;
}

#define WSPACE      " \r\n\t\f"

xtree_scan_token_t *xtree_gtml_scan_wspace (struct xtree_errobj_t *err, xtree_reader_t *reader)
{
  xtree_scan_token_t *ret = NULL;
  unsigned char *text = NULL;
  ds_strbuilder_t *sb = NULL;

  if (!(sb = ds_strbuilder_new (NULL))) {
    ERROR (err, xtree_errcode_OOM, "OOM allocating string builder");
    goto cleanup;
  }

  int c;
  unsigned char tmp[2] = { 0, 0 };
  while ((c = xtree_reader_peek (reader)) &&
         ((strchr (WSPACE, c)) != NULL)) {
    tmp[0] = (unsigned char)xtree_reader_get (reader);
    if (!(ds_strbuilder_append (sb, tmp, NULL))) {
      ERROR (err, xtree_errcode_OOM, "OOM appending to string builder");
      goto cleanup;
    }
  }

  if (!(text = U(ds_strbuilder_build (sb)))) {
    ERROR (err, xtree_errcode_OOM, "String builder construction failure");
    goto cleanup;
  }

  if (!(ret = xtree_scan_token_new (xtree_gtml_token_WSPACE,
                                    xtree_reader_name (reader),
                                    xtree_reader_line (reader),
                                    xtree_reader_cpos (reader),
                                    text, strlen (C(text))))) {
    ERROR (err, xtree_errcode_OOM,
           "Failed to construct token %i [%s]", xtree_gtml_token_WSPACE, text);
    goto cleanup;
  }

cleanup:
  free (text);
  ds_strbuilder_del (sb);
  if (xtree_errchk (err)) {
    xtree_scan_token_free (&ret);
  }
  return ret;
}

xtree_scan_token_t *collect_single (struct xtree_errobj_t *err,
                                    xtree_reader_t *reader,
                                    int id,
                                    uint64_t flags,
                                    int expected)
{
  xtree_scan_token_t *ret = NULL;
  unsigned char text[2] = { 0, 0 };

  while ((flags & F_CONSUME_WSPACE) && isspace (xtree_reader_peek (reader))) {
    xtree_reader_get (reader);
  }

  if ((xtree_reader_peek (reader)) != expected) {
    ERROR (err, xtree_errcode_UNEXPECTED_TOKEN,
           "Expected [%c], got [%c]", expected, xtree_reader_peek (reader));
    goto cleanup;
  }
  text[0] = (unsigned char)xtree_reader_get (reader);

  if (!(ret = xtree_scan_token_new (id,
                                    xtree_reader_name (reader),
                                    xtree_reader_line (reader),
                                    xtree_reader_cpos (reader),
                                    text, strlen (C(text))))) {
    ERROR (err, xtree_errcode_OOM,
           "Failed to construct token %i [%s]", id, text);
    goto cleanup;
  }


cleanup:
  if (xtree_errchk (err)) {
    xtree_scan_token_free (&ret);
  }
  return ret;
}

xtree_scan_token_t *xtree_gtml_scan_equal (struct xtree_errobj_t *err, xtree_reader_t *reader)
{
  return collect_single (err, reader, xtree_gtml_token_EQUAL,
                         0, '=');
}

xtree_scan_token_t *xtree_gtml_scan_gt (struct xtree_errobj_t *err, xtree_reader_t *reader)
{
  return collect_single (err, reader, xtree_gtml_token_GT,
                         F_CONSUME_WSPACE, '>');
}

xtree_scan_token_t *xtree_gtml_scan_cdata (struct xtree_errobj_t *err, xtree_reader_t *reader)
{
  return collect_between (err, reader, xtree_gtml_token_CDATA, F_CONSUME_BOTH,
                          U("<![CDATA["), U("]]>"));
}

xtree_scan_token_t *xtree_gtml_scan_tagname (struct xtree_errobj_t *err, xtree_reader_t *reader)
{
  return collect_until (err, reader, xtree_gtml_token_TAGNAME, F_CONSUME_BEGIN,
                        U("<"), U(">/" WSPACE));
}

xtree_scan_token_t *xtree_gtml_scan_close_tag (struct xtree_errobj_t *err, xtree_reader_t *reader)
{
  xtree_scan_token_t *ret = collect_until (err, reader, xtree_gtml_token_CLOSE_TAG,
                                           F_CONSUME_BEGIN,
                                           U("</"), U(">" WSPACE));
  int c = 0;
  while (((c = xtree_reader_peek (reader)) != '>') &&
          (strchr (C(WSPACE), c))) {
    xtree_reader_get (reader);
  }
  if ((xtree_reader_peek (reader)) != '>') {
    E (reader, xtree_errcode_UNEXPECTED_TOKEN, "unclosed tag");
    xtree_scan_token_free (&ret);
  }
  return ret;
}

xtree_scan_token_t *xtree_gtml_scan_sclosing (struct xtree_errobj_t *err, xtree_reader_t *reader)
{
  return collect_until (err, reader, xtree_gtml_token_SCLOSING,
                        F_CONSUME_BOTH | F_CONSUME_WSPACE,
                        U(""), U("/>"));
}

xtree_scan_token_t *xtree_gtml_scan_attr_name (struct xtree_errobj_t *err, xtree_reader_t *reader)
{
  xtree_scan_token_t *wspace = xtree_gtml_scan_wspace (err, reader);
  xtree_scan_token_free (&wspace);
  if (xtree_errchk (err))
  {
    return NULL;
  }

  return collect_until (err, reader, xtree_gtml_token_ATTR_NAME, F_CONSUME_BEGIN,
                        U (""), U ("=/>" WSPACE));
}

xtree_scan_token_t *xtree_gtml_scan_attr_value (struct xtree_errobj_t *err, xtree_reader_t *reader)
{
  const unsigned char *end = U(WSPACE "/>");
  unsigned char start[2] = { 0, 0 };

  int c = xtree_reader_peek (reader);
  if (c == '"' || c == '\'') {
    start[0] = (unsigned char)c;
    end = start;
    return collect_until (err, reader, xtree_gtml_token_ATTR_VALUE,
                          F_CONSUME_BOTH,
                          start, end);
  }

  xtree_scan_token_t *ret = collect_until (err, reader, xtree_gtml_token_ATTR_VALUE,
                                           F_CONSUME_BEGIN,
                                           start, end);
  return ret;
}

xtree_scan_token_t *xtree_gtml_scan_content (struct xtree_errobj_t *err, xtree_reader_t *reader)
{
  return collect_until (err, reader, xtree_gtml_token_CONTENT,
                        F_CONSUME_BEGIN | F_ALLOW_EOF,
                        U (""), U ("<&"));
}

xtree_scan_token_t *xtree_gtml_scan_ecode (struct xtree_errobj_t *err, xtree_reader_t *reader)
{
  return collect_until (err, reader, xtree_gtml_token_ECODE, F_CONSUME_BOTH,
                        U ("&"), U (";"));
}

xtree_scan_token_t *xtree_gtml_scan_comment (struct xtree_errobj_t *err, xtree_reader_t *reader)
{
  return collect_between (err, reader, xtree_gtml_token_COMMENT, F_CONSUME_BOTH,
                          U ("<!--"), U ("-->"));
}

xtree_scan_token_t *xtree_gtml_scan_directive (struct xtree_errobj_t *err, xtree_reader_t *reader)
{
  return collect_between (err, reader, xtree_gtml_token_DIRECTIVE, F_CONSUME_BOTH,
                          U ("<?"), U ("?>"));
}


