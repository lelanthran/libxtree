#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "ds_str.h"
#include "ds_strbuilder.h"

#include "xtree_err.h"
#include "xtree.h"
#include "xtree_reader.h"
#include "xtree_scan.h"
#include "xtree_gtml_scan.h"

#include "xtree_gtml_parse.h"

#if 1
#define DIAG(msg,token)     do {\
  printf ("%8s: %s\n", \
          xtree_scan_token_text (token), msg); \
} while (0)
#endif


#define E(e,c,r,...)    do { \
  unsigned char *tmp = NULL; \
  ds_str_printf ((char **)&tmp, __VA_ARGS__); \
  ERROR (e, c, "%s:%zu:%zu: Error near\n---\n%s\n---\n%s", \
         xtree_reader_name (r), \
         xtree_reader_line (r), \
         xtree_reader_cpos (r), \
         xtree_reader_snippet (r, 64), \
         tmp); \
  free (tmp); \
} while (0);






/* *************************************************************************** *
 * Convenience/helper functions.
 */

static bool chkerr (struct xtree_errobj_t const *err)
{
  return err->libcode || err->syscode ? true : false;
}






/* *************************************************************************** *
 * Scanner functions
 */

#define SKIPWS_GET    (1 << 0)
#define SKIPWS_PEEK   (1 << 1)

static int scan_skipws (xtree_reader_t *reader, uint32_t flags)
{
  int ret = 0;

  while ((ret = xtree_reader_peek (reader)) &&
         (isspace (ret))) {
    xtree_reader_get (reader);
  }

  return flags & SKIPWS_GET ? xtree_reader_get (reader) : ret;
}

static unsigned char *scan_id (struct xtree_errobj_t *err,
                               xtree_reader_t *reader)
{
  static const unsigned char *allowed_start = (const unsigned char *)
    "ABCDEFGHIJKLMNOPRSTUVWXYZ"
    "abcdefghijklmnoprstuvwxyz"
    "~!@$%^*()_+{}|-[]."
    "";
  static const unsigned char *allowed_in = (const unsigned char *)
    "ABCDEFGHIJKLMNOPRSTUVWXYZ"
    "abcdefghijklmnoprstuvwxyz"
    "~!@$%^*()_+{}|-[]."
    "0123456789"
    "";

  const unsigned char *allowed = allowed_start;

  unsigned char *ret = NULL;
  ds_strbuilder_t *sb = ds_strbuilder_new (NULL);
  int c = 0;
  unsigned char tmp[2] = { 0, 0 };

  while ((c = xtree_reader_peek (reader)) &&
         (c != EOF)) {
    if ((strchr ((const char *)allowed, c)) == NULL)
        break;
    c = xtree_reader_get (reader);
    tmp[0] = (unsigned char)c;
    if (!(ds_strbuilder_append (sb, tmp, NULL))) {
      E (err, xtree_errcode_OOM, reader, "OOM appending %c to string", c);
      goto cleanup;
    }
    allowed = allowed_in;
  }

  if (!(ret = (unsigned char *)ds_strbuilder_build (sb))) {
    E (err, xtree_errcode_OOM, reader, "OOM constructing string");
    goto cleanup;
  }
  if (ret[0] == 0) {
    E (err, xtree_errcode_UNEXPECTED_TOKEN, reader,
       "Expected to scan ID, no ID found in next token");
    goto cleanup;
  }

cleanup:
  ds_strbuilder_del (sb);
  if (chkerr (err)) {
    free (ret);
    ret = NULL;
  }
  return ret;
}

static unsigned char *scan_string (struct xtree_errobj_t *err,
                                   xtree_reader_t *reader)
{
  ds_strbuilder_t *sb = ds_strbuilder_new (NULL);
  unsigned char *ret = NULL;
  int delim = xtree_reader_get (reader);
  int c;

  if (!sb) {
    E (err, xtree_errcode_OOM, reader, "Out of memory consuming string");
    goto cleanup;
  }

  unsigned char tmp[2] = { 0, 0 };

  while (((c = xtree_reader_get (reader)) > 0) &&
          (c != delim)) {
    tmp[0] = (unsigned char)c;
    if (!(ds_strbuilder_append (sb, tmp, NULL))) {
      E (err, xtree_errcode_OOM, reader, "Out of memory appending string");
      goto cleanup;
    }
  }

  if (c != delim) {
    E (err, xtree_errcode_UNTERMINATED_STRING, reader, "Unterminated string");
    goto cleanup;
  }

  if (!(ds_strbuilder_append (sb, (unsigned char *)"\"", NULL))) {
    E (err, xtree_errcode_OOM, reader, "Out of memory terminating string");
    goto cleanup;
  }

  if (!(ret = (unsigned char *)ds_strbuilder_build (sb))) {
    E (err, xtree_errcode_OOM, reader, "Failed to build string");
    goto cleanup;
  }

cleanup:
  ds_strbuilder_del (sb);
  if (chkerr (err)) {
    free (ret);
    ret = NULL;
  }

  return ret;
}


static xtree_node_t *tag_new (struct xtree_errobj_t *err,
                              xtree_reader_t *reader,
                              xtree_node_t *parent)
{
  xtree_node_t *ret = NULL;
  unsigned char *tagname = NULL;

  scan_skipws (reader, SKIPWS_GET);

  if (!(tagname = scan_id (err, reader)))
    goto cleanup;

  if (!(ret = xtree_node_new (err, parent, tagname, xtree_node_type_LIST)))
    goto cleanup;

cleanup:
  free (tagname);
  if (chkerr (err)) {
    xtree_node_free (&ret);
  }
  return ret;
}

static bool tag_parse_attr (struct xtree_errobj_t *err,
                            struct xtree_reader_t *reader,
                            struct xtree_node_t *parent)
{
  unsigned char *name = NULL;
  unsigned char *value = NULL;
  int c = 0;

  (void)err;
  (void)parent;

  scan_skipws (reader, SKIPWS_GET);

  if (!(name = scan_id (err, reader)))
    goto cleanup;

  c = scan_skipws (reader, SKIPWS_PEEK);
  if ((c != '=') && (c != '/') && (c != '>')) {
    E (err, xtree_errcode_UNEXPECTED_TOKEN, reader,
       "[%c] Unexpected token, expected '=', '/' or '>', got '%c'", c, c);
    goto cleanup;
  }

  c = xtree_reader_get (reader);
  if (c != '=') {
    // End parsing of attributes
    goto cleanup;
  }

  c = xtree_reader_peek (reader);

  switch (c) {
    case '\'':
    case '"':
      if (!(value = scan_string (err, reader)))
        goto cleanup;
      break;

    default:
      if (!(value = (unsigned char *)ds_str_dup ("")))
        goto cleanup;

      c = scan_skipws (reader, SKIPWS_PEEK);
      if (c != '<' && c != '/') {
        E (err, xtree_errcode_UNEXPECTED_TOKEN, reader,
           "Expected token [%c]", c);
        goto cleanup;
      }
      if (!(value = scan_id (err, reader)))
        goto cleanup;
  }

  if (!(xtree_node_attr_value_set (err, parent, name, value)))
    goto cleanup;

cleanup:
  free (name);
  free (value);
  return !chkerr (err);
}

static void tag_parse_attrs (struct xtree_errobj_t *err,
                             struct xtree_reader_t *reader,
                             struct xtree_node_t *parent)
{
  while ((tag_parse_attr (err, reader, parent)) == true)
    ;
}

static bool tag_parse_innertext (struct xtree_errobj_t *err,
                                 struct xtree_reader_t *reader,
                                 struct xtree_node_t *parent)
{
  (void)err;
  (void)reader;
  (void)parent;
  E (err, xtree_errcode_UNIMPLEMENTED, reader, "[%s]: Not implemented", __func__);
  return false;
}

static xtree_node_t *innertext_new (struct xtree_errobj_t *err,
                                    xtree_reader_t *reader,
                                    xtree_node_t *parent,
                                    int current)
{
  (void)err;
  (void)reader;
  (void)parent;
  (void)current;
  E (err, xtree_errcode_UNIMPLEMENTED, reader, "[%s]: Not implemented", __func__);
  return NULL;
}

/* ****************************************************************************
 * All tags are of type LIST, all text is of type ATOM.

Originally I proposed a separate scanner step, but SGML tokens are not context
free as JSON is. Look at the following problem:

 <tag a="b"> c="d" </tag>
   { TAG_START_OPEN,   "tag" }
   { TAG_TEXT,         "a"   }
   { TAG_EQUALS,       "="   }
   { TAG_STRING,       "b"   }
   { TAG_START_CLOSE,  ""    }

This is senseless - the scanner cannot tell if something is an ID or a
string.

Better to do scanning and parsing in a single step, because we need to maintain
context while scanning:

switch ((c = read (reader))) {
  case '<':
    if ((peek (reader)) != '/') {
      tagname = read_tag (reader);
    } else {
      // End current parse
    }
    break;
  default:
    accumulate (&text, read_text (reader));
    break;
}
 */

static xtree_node_t *toplevel_parse (struct xtree_errobj_t *err,
                                     xtree_node_t *parent,
                                     xtree_reader_t *reader,
                                     size_t depth,
                                     size_t max_depth)
{
  /* ***************************************************************************
   * At the toplevel, parsing is simple:
   *    while ((c = reader_peek (...)) && c != EOF && reader_lookahead (la)) {
   *        if (la == "<![CDATA[") {
   *          tree_child_append (parent, scan_cdata);
   *          continue;
   *        }
   *        if (la == "</") {
   *          if ((tagname = scan_tagname (...)) != parent.tagname) {
   *            // ERROR
   *          }
   *          break; // We return a completed tree here: parent.
   *        }
   *        if (c == '<') {
   *          tree_new (parent, ...);
   *        }
   *        default: tree_child_append (parent, scan_content (...) break;
   *      }
   *    }
   *
   * `tree_new()` is a bit complex; we have scan_content (for content that
   * will return whitespace tokens, codes and case-preserving everything else,
   * except '<') and scan_tag (for tag metadata, that recognises strings,
   * slashes, `<`, `>`, and uppercases all IDENT types, etc).
   *
   *    ret = tree_node_new (parent, scan_tagname (...)); // Read the tagname
   *    while ((name = scan_attr_name (...))) {
   *      if (name.type == LT || name.type == SLASH)
   *        break;
   *      tmp = scan_attr_value (...);
   *      if (tmp.type == LT || tmp.type == SLASH) {
   *        tree_attr_add (parent, name, "");
   *        break;
   *      }
   *      tree_attr_add (parent, name, value);
   *    } // Done setting up attributes and tagname, continue appending content
   *
   *    while ((token = scan_content (...))) {
   *      if (token.value[0] == ';') {
   *        scan_ecode (&token);
   *      }
   *      tree_child_attach (parent, token);
   *    }
   *
   */
  xtree_node_t *ret = NULL;
  int current = 0, next = 0;

  if (depth >= max_depth) {
    E (err, xtree_errcode_DEPTH_EXCEEDED, reader,
       "Depth %zu exceeds max permissable depth %zu",
       depth, max_depth);
    goto cleanup;
  }

  current = scan_skipws (reader, SKIPWS_GET);
  next = xtree_reader_peek (reader);
  switch (current) {
    case '<':
      if (next == '/') {
        // Close tag found, read the ID, ensure it matches then break;
        break;
      }
      if (!(ret = tag_new (err, reader, parent)))
        goto cleanup;
      current = scan_skipws (reader, SKIPWS_GET);
      if (current <= 0) {
        E (err, xtree_errcode_EOF, reader, "Unexpected EOF");
        goto cleanup;
      }
      // If the opening tag is now complete, we scan the inner content
      if (current == '>') {
        if (!(toplevel_parse (err, ret, reader, depth + 1, max_depth)))
          goto cleanup;
      }
      // If the tag is self closing, this tag has no children, we return
      if (current == '/') {
        current = scan_skipws (reader, SKIPWS_GET);
        if (current != '>') {
          E (err, xtree_errcode_UNEXPECTED_TOKEN, reader,
                  "Unexpected token found");
          goto cleanup;
        }
      }
      // Parse attributes, if any. False is returned only on error. When there
      // are no attributes, true is returned
      tag_parse_attrs (err, reader, ret);
      if (!(tag_parse_innertext (err, reader, ret))) {
        goto cleanup;
      }
      break;

    default:
      if (!ret) {
        if (!(ret = innertext_new (err, reader, parent, current))) {
          goto cleanup;
        }
      }
      break;
  }

cleanup:
  if (err->libcode || err->syscode) {
    xtree_node_free (&ret);
  }
  return ret;
}
















xtree_node_t *xtree_gtml_parse (struct xtree_errobj_t *err,
                                xtree_node_t *parent,
                                xtree_reader_t *reader,
                                size_t max_depth)
{
  /* **************************************************************************
   * This toplevel function cannot be recursive; need a recursive function to
   * actually do the recursion, with a depth of zero
   *
   */

  xtree_errobj_reset (err, 1024);
  return toplevel_parse (err, parent, reader, 0, max_depth);
}



static bool gtml_dump (struct xtree_errobj_t *err, const xtree_node_t *node,
                       ds_strbuilder_t *sb, size_t indent, size_t level);

#if 0
static bool dump_indent (struct xtree_errobj_t *err,
                         ds_strbuilder_t *sb,
                         size_t indent, size_t level)
{
  for (size_t i=0; i < (level * indent); i++) {
    if (!(ds_strbuilder_append (sb, (const unsigned char *)" ", NULL))) {
      ERROR (err, xtree_errcode_OOM, "OOM dumping indentation");
      return false;
    }
  }
  return true;
}

static bool dump_atom (struct xtree_errobj_t *err, const xtree_node_t *node,
                       ds_strbuilder_t *sb, size_t indent, size_t level)
{
  bool ret = true;
  (void)indent;
  (void)level;
  ret = ret && ds_strbuilder_append (sb, (const unsigned char *)" ", NULL);
  ret = ret && ds_strbuilder_append (sb,
                                     xtree_node_value_get (err, node),
                                     NULL);
  return ret;
}

static bool dump_string (struct xtree_errobj_t *err, const xtree_node_t *node,
                         ds_strbuilder_t *sb, size_t indent, size_t level)
{
  bool ret = true;
  (void)indent;
  (void)level;
  ret = ret && ds_strbuilder_append (sb, (const unsigned char *)" ", NULL);
  ret = ret && ds_strbuilder_append (sb,
                                     (const unsigned char *)"\"",
                                     xtree_node_value_get (err, node),
                                     (const unsigned char *)"\"",
                                     NULL);
  return ret;
}
#endif

static bool gtml_dump (struct xtree_errobj_t *err, const xtree_node_t *node,
                       ds_strbuilder_t *sb, size_t indent, size_t level)
{
  /* TODO: Determine what the format is going to be for storing XML in
   * xtree_node_t objects.
   */
  (void)err;
  (void)node;
  (void)sb;
  (void)indent;
  (void)level;
  bool error = true;

  return !error;
}

char *xtree_gtml_parse_dump (struct xtree_errobj_t *err,
                             const xtree_node_t *gtml_node,
                             size_t indent)
{
  ds_strbuilder_t *sb = ds_strbuilder_new (NULL);
  if (!(gtml_dump (err, gtml_node, sb, indent, 0))) {
    ds_strbuilder_del (sb);
    return NULL;
  }
  char *ret = ds_strbuilder_build (sb);
  ds_strbuilder_del (sb);
  return ret;
}


