#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

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



static void token_errv (struct xtree_errobj_t *err,
                        xtree_scan_token_t *token,
                        enum xtree_errcode_t errcode,
                        const char *fmts,
                        va_list ap)
{
  char *tmp1 = NULL,
       *tmp2 = NULL,
       *msg = NULL;

  ds_str_printf (&tmp1, "[%s:%i] [%s] : ",
                 xtree_scan_token_filename (token),
                 xtree_scan_token_line (token),
                 xtree_scan_token_text (token));
  ds_str_vprintf (&tmp2, fmts, ap);

  if (!tmp1 || !tmp2 || !(msg = ds_str_cat (tmp1, tmp2, NULL))) {
    ERROR (err, xtree_errcode_OOM, "OOM");
  } else {
    ERROR (err, errcode, "%s", msg);
  }
  free (tmp1);
  free (tmp2);
  free (msg);
}

static void token_err (struct xtree_errobj_t *err,
                       xtree_scan_token_t *token,
                       enum xtree_errcode_t errcode,
                       const char *fmts,
                       ...)
{
  va_list ap;
  va_start (ap, fmts);
  token_errv (err, token, errcode, fmts, ap);
  va_end (ap);
}

static bool token_id_assertv (struct xtree_errobj_t *err,
                              xtree_scan_token_t *token,
                              enum xtree_gtml_token_id_t id, va_list ap)
{
  enum xtree_gtml_token_id_t tokenid = xtree_scan_token_id (token);
  while (id != xtree_gtml_token_RESERVED) {
    if (id == tokenid)
      return true;
    id = va_arg (ap, enum xtree_gtml_token_id_t);
  }
  token_err (err, token, xtree_errcode_UNEXPECTED_TOKEN, "Unexpected token");
  return false;
}

static bool token_id_assert (struct xtree_errobj_t *err,
                             xtree_scan_token_t *token,
                             enum xtree_gtml_token_id_t id, ...)
{
  va_list ap;
  va_start (ap, id);
  bool ret = token_id_assertv (err, token, id, ap);
  va_end (ap);
  return ret;
}






static xtree_node_t *parse_object (struct xtree_errobj_t *err,
                                   xtree_node_t *parent,
                                   xtree_reader_t *reader,
                                   size_t *depth,
                                   size_t max_depth);

static xtree_node_t *parse_array (struct xtree_errobj_t *err,
                                  xtree_node_t *parent,
                                  xtree_reader_t *reader,
                                  size_t *depth,
                                  size_t max_depth);

static xtree_node_t *toplevel_parse (struct xtree_errobj_t *err,
                            xtree_node_t *parent,
                            xtree_reader_t *reader,
                            size_t depth,
                            size_t max_depth);

















/* ****************************************************************************
 * Nodes have type (atom/list), name (optional) and value { text | children }.
 *
 * Node type:
 * 1. A node-type==atom stores a single text token
 * 2. A node-type==list stores either Object or Array
 *
 * Node name:
 * 1. The name field stores the tml-type: string, number, symbol, object,
 *    array, true, false, NaN, comment, etc.
 *
 * Node value:
 * 1. When node-type==atom, the value is stored as text.
 * 2. When node-type==list and tml-type==object, value is stored as children:
 *    - Each child node is a single field, with node-type==list
 *    - Each field has tml-type=="field"
 *    - Each field.child[0]==node(atom, name==tml-type, value = text)
 *    - Each field.child[1] is the node (same as root node, but with a parent).
 *  3. When node-type==list and tml-type==array, value is stored as children:
 *    - Each child node is a node (same as root node, but with a parent).
 *
 *
 * Parsing top-level (entry-point into parsing, using a parent parameter):
 * 1. When encountering a scalar token, append it as a child-node to parent as
 *      node(atom, tml-type, value).
 * 2. When encountering an object-start, append a new child-node to parent as
 *      node(list, "object"),
 *    then append each field of type node as node(list, "field", parse(reader))
 * 3. When encountering an array start, append a new child-node to parent as
 *    node(list, "array"), then append each child-node to array as
 *    node(list, "array", parse(reader)).
 * 4. Anything else encountered in the top-level is an error.
 *
 * Parsing objects (given a `object` node):
 * 1. Scan a complete field into a node(list, "field").
 * 2. If non-null node is returned, append the field as a child to the
 *    `object`, else end.
 *
 * Parsing fields (given a `field` node):
 * 1. Scan optional comma token, discard.
 * 2. Scan fieldname as node(list, tml-type, <fieldname>) and append to
 *    `field` node.
 * 3. Scan mandatory colon and discard.
 * 4. Scan fieldvalue as parse(reader) and append to `field` node.
 */


static xtree_node_t *parse (struct xtree_errobj_t *err,
                            xtree_node_t *parent,
                            xtree_reader_t *reader,
                            xtree_scan_token_t *token,
                            size_t *depth,
                            size_t max_depth)
{
  xtree_node_t *ret = NULL;
  enum xtree_gtml_token_id_t tokenid = xtree_scan_token_id (token);

  if (*depth >= max_depth) {
    ERROR (err, xtree_errcode_DEPTH_EXCEEDED,
           "depth exceeded: %zu (depth) >= %zu (max)", *depth, max_depth);
    return NULL;
  }
  *depth = *depth + 1;

  switch (tokenid) {
    case xtree_gtml_token_LBRACE:
      ret = parse_object (err, parent, reader, depth, max_depth);
      break;

    case xtree_gtml_token_LBRACKET:
      ret = parse_array (err, parent, reader, depth, max_depth);
      break;

#define ATOM(jtype,token) \
  xtree_node_new_atom (err, \
                       parent, \
                       (const unsigned char *)jtype, \
                       xtree_scan_token_text (token), \
                       NULL);

    case xtree_gtml_token_INFINITY:
    case xtree_gtml_token_NEG_INFINITY:
    case xtree_gtml_token_NAN:
    case xtree_gtml_token_NEG_NAN:
    case xtree_gtml_token_NUMBER:
      ret = ATOM ("number", token);
      break;

    case xtree_gtml_token_STRING:
      ret = ATOM ("string", token);
      break;

    case xtree_gtml_token_FALSE:
    case xtree_gtml_token_TRUE:
      ret = ATOM ("bool", token);
      break;

    case xtree_gtml_token_UNDEFINED:
    case xtree_gtml_token_NULL:
      ret = ATOM ("reference", token);
      break;

    case xtree_gtml_token_COMMENT:
      ret = ATOM ("comment", token);
      break;

    case xtree_gtml_token_SYMBOL:
      ret = ATOM ("symbol", token);
      break;

    default:
      break;
  }
#undef ATOM


  return ret;
}

static xtree_node_t *parse_field (struct xtree_errobj_t *err,
                                  xtree_reader_t *reader,
                                  size_t *fieldnum,
                                  size_t *depth,
                                  size_t max_depth)
{
  bool error = true;
  xtree_node_t *ret = NULL;

  xtree_scan_token_t *token_name = NULL;
  xtree_scan_token_t *token_colon = NULL;
  xtree_scan_token_t *token_value = NULL;

  // The first token we encounter might by a comma (from a previous field) or a
  // closing brace (ending the field list).
  if (!(token_name = xtree_gtml_scan (reader))) {
    token_err (err, token_name, xtree_errcode_EOF,
               "Expected a token, got NULL");
    goto cleanup;
  }
  enum xtree_gtml_token_id_t token_name_id = xtree_scan_token_id (token_name);

  // Check if the token_name was a comma, which is allowed only when
  // fieldnum != 0, and allowed even if the next token is a '}'.
  if (*fieldnum && token_name_id == xtree_gtml_token_COMMA) {
    xtree_scan_token_free (&token_name);
    if (!(token_name = xtree_gtml_scan (reader))) {
      token_err (err, token_name, xtree_errcode_EOF,
                 "Expected a token, got NULL");
      goto cleanup;
    }
  }
  // If token is a closing brace, the field list has ended, we return no field
  // with no error.
  token_name_id = xtree_scan_token_id (token_name);
  if (token_name_id == xtree_gtml_token_RBRACE) {
    goto cleanup;
  }

  // The next 2x tokens are mandatory, even though the colon is discarded.
  if (!(token_colon = xtree_gtml_scan (reader)) ||
      !(token_value = xtree_gtml_scan (reader))) {
    token_err (err, token_colon ? token_colon : token_value,
                    xtree_errcode_EOF, "Expected a token, got NULL");
    goto cleanup;
  }

  // Check that all the tokens are of the correct type; our implementation
  // allows a very relaxed JSON fieldname; it can be a string OR a symbol.
  if (!(token_id_assert (err, token_name,
                         xtree_gtml_token_STRING,
                         xtree_gtml_token_SYMBOL,
                         xtree_gtml_token_RESERVED)) ||
      !(token_id_assert (err, token_colon,
                         xtree_gtml_token_COLON,
                         xtree_gtml_token_RESERVED)) ||
      !(token_id_assert (err, token_value,
                         xtree_gtml_token_STRING,
                         xtree_gtml_token_SYMBOL,
                         xtree_gtml_token_NUMBER,
                         xtree_gtml_token_NAN,
                         xtree_gtml_token_INFINITY,
                         xtree_gtml_token_NEG_NAN,
                         xtree_gtml_token_NEG_INFINITY,
                         xtree_gtml_token_TRUE,
                         xtree_gtml_token_FALSE,
                         xtree_gtml_token_NULL,
                         xtree_gtml_token_UNDEFINED,
                         xtree_gtml_token_LBRACKET,
                         xtree_gtml_token_LBRACE,
                         xtree_gtml_token_RESERVED))) {
    goto cleanup;
  }

  xtree_node_t *node_name = NULL;
  if (!(ret = xtree_node_new (err, NULL,
                              (const unsigned char *)"field",
                              xtree_node_type_LIST)) ||
      !(node_name = xtree_node_new (err, ret,
                                    xtree_scan_token_text (token_name),
                                    xtree_node_type_ATOM))) {
    goto cleanup;
  }

  if (!(parse (err, ret, reader, token_value, depth, max_depth)))
    goto cleanup;

  error = false;

cleanup:
  xtree_scan_token_free (&token_name);
  xtree_scan_token_free (&token_colon);
  xtree_scan_token_free (&token_value);
  if (error) {
    xtree_node_free (&ret);
  }
  return ret;
}

static xtree_node_t *parse_object (struct xtree_errobj_t *err,
                                   xtree_node_t *parent,
                                   xtree_reader_t *reader,
                                   size_t *depth,
                                   size_t max_depth)
{
  /* We have the first token '{'. We should then sit in a loop collecting
   * fields until we see the closing '}'. `parse_field()` returns NULL on
   * either EOF or `}`. Caller must check `err` to determine if NULL was
   * returned due to error or due to '}'.
   */
  xtree_node_t *ret = NULL;
  xtree_node_t *field = NULL;

  if (!(ret = xtree_node_new (err,
                              parent,
                              (unsigned char *)"object",
                              xtree_node_type_LIST))) {
    return NULL;
  }

  size_t fnum = 0;
  while ((field = parse_field (err, reader, &fnum, depth, max_depth))) {
    fnum++;
    if (!(xtree_node_child_append (err, ret, field)))
      break;
  }

  return ret;
}

static xtree_node_t *parse_array (struct xtree_errobj_t *err,
                                  xtree_node_t *parent,
                                  xtree_reader_t *reader,
                                  size_t *depth,
                                  size_t max_depth)
{
  /* We already have the first token, the '['. We sit in a loop adding
   * nameless values to `ret`, until we get EOF or error (either result in an
   * error) or until a `]` (which ends without an error.
   */
  bool error = true;
  xtree_node_t *ret = NULL;
  xtree_scan_token_t *token = NULL;
  enum xtree_gtml_token_id_t tokenid = xtree_gtml_token_RESERVED;

  if (!(ret = xtree_node_new (err, parent, (unsigned char *)"array", xtree_node_type_LIST))) {
    goto cleanup;
  }

  while ((token = xtree_gtml_scan (reader)) &&
         (tokenid = xtree_scan_token_id (token)) &&
         (tokenid != xtree_gtml_token_RBRACKET)) {
    if (tokenid == xtree_gtml_token_COMMA) {
      xtree_scan_token_free (&token);
      continue;
    }
    if (!(token_id_assert (err, token,
                           xtree_gtml_token_STRING,
                           xtree_gtml_token_SYMBOL,
                           xtree_gtml_token_NUMBER,
                           xtree_gtml_token_NAN,
                           xtree_gtml_token_INFINITY,
                           xtree_gtml_token_NEG_NAN,
                           xtree_gtml_token_NEG_INFINITY,
                           xtree_gtml_token_TRUE,
                           xtree_gtml_token_FALSE,
                           xtree_gtml_token_NULL,
                           xtree_gtml_token_UNDEFINED,
                           xtree_gtml_token_LBRACKET,
                           xtree_gtml_token_LBRACE,
                           xtree_gtml_token_RESERVED))) {
        goto cleanup;
    }
    if (!(parse (err, ret, reader, token, depth, max_depth)))
      goto cleanup;
    xtree_scan_token_free (&token);
  }

  if (!token || (tokenid == xtree_gtml_token_EOF)) {
    token_err (err, token, xtree_errcode_EOF,
               "Unexpected EOF while reading array");
    goto cleanup;
  }

  error = false;
cleanup:
  xtree_scan_token_free (&token);
  if (error) {
    xtree_node_free (&ret);
  }
  return ret;
}


static xtree_node_t *toplevel_parse (struct xtree_errobj_t *err,
                                     xtree_node_t *parent,
                                     xtree_reader_t *reader,
                                     size_t depth,
                                     size_t max_depth)
{
  xtree_node_t *ret = NULL;
  xtree_scan_token_t *token = xtree_gtml_scan (reader);

  xtree_errobj_reset (err, 1024);
  if (!token) {
    ERROR (err, xtree_errcode_OOM, "Possible OOM");
    goto cleanup;
  }

#define E(c,msg)    do { \
  token_err (err, token, c, "%s:%zu: " msg); \
} while (0)

  switch (xtree_scan_token_id (token)) {
    case xtree_gtml_token_ERROR:
      E (xtree_errcode_UNKNOWN, "Unexpected error in input");
      break;

    case xtree_gtml_token_EOF:
      // E (xtree_errcode_EOF, "EOF in input");
      break;

    case xtree_gtml_token_RBRACE:
      E (xtree_errcode_UNEXPECTED_TOKEN, "Unexpected closing brace");
      break;

    case xtree_gtml_token_RBRACKET:
      E (xtree_errcode_UNEXPECTED_TOKEN, "Unexpected closing bracket");
      break;

    case xtree_gtml_token_COLON:
      E (xtree_errcode_UNEXPECTED_TOKEN, "Unexpected colon");
      break;

    case xtree_gtml_token_COMMA:
      E (xtree_errcode_UNEXPECTED_TOKEN, "Unexpected comma");
      break;

    case xtree_gtml_token_UNRECOGNISED:
      E (xtree_errcode_UNEXPECTED_TOKEN, "Unrecognised token");
      break;

    /* ********************************************************************** */

#define ATOM(jtype,token) \
  xtree_node_new_atom (err,\
                       parent,\
                       (const unsigned char *)jtype,\
                       xtree_scan_token_text (token), \
                       NULL);

    case xtree_gtml_token_INFINITY:
    case xtree_gtml_token_NEG_INFINITY:
    case xtree_gtml_token_NAN:
    case xtree_gtml_token_NEG_NAN:
    case xtree_gtml_token_NUMBER:
      ret = ATOM ("number", token);
      break;

    case xtree_gtml_token_STRING:
      ret = ATOM ("string", token);
      break;

    case xtree_gtml_token_FALSE:
    case xtree_gtml_token_TRUE:
      ret = ATOM ("bool", token);
      break;

    case xtree_gtml_token_UNDEFINED:
    case xtree_gtml_token_NULL:
      ret = ATOM ("reference", token);
      break;

    case xtree_gtml_token_COMMENT:
      ret = ATOM ("comment", token);
      break;

    case xtree_gtml_token_SYMBOL:
      ret = ATOM ("symbol", token);
      break;

    /* ********************************************************************** */

    case xtree_gtml_token_LBRACE:
      ret = parse_object (err, parent, reader, &depth, max_depth);
      break;

    case xtree_gtml_token_LBRACKET:
      ret = parse_array (err, parent, reader, &depth, max_depth);
      break;

  }

#undef ATOM
#undef E

cleanup:
  if (err->libcode || err->syscode) {
    xtree_node_free (&ret);
  }
  xtree_scan_token_free (&token);
  return ret;
}
















xtree_node_t *xtree_gtml_parse (struct xtree_errobj_t *err,
                                xtree_reader_t *reader,
                                size_t max_depth)
{
  /* **************************************************************************
   * This toplevel function cannot be recursive; need a recursive function to
   * actually do the recursion.
   *
   */

  return toplevel_parse (err, NULL, reader, 0, max_depth);
}



static bool tml_dump (struct xtree_errobj_t *err, const xtree_node_t *node,
                       ds_strbuilder_t *sb, size_t indent, size_t level);

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

static bool dump_key (struct xtree_errobj_t *err, const xtree_node_t *node,
                      ds_strbuilder_t *sb, size_t indent, size_t level)
{
  bool ret = true;
  ret = ret && dump_indent (err, sb, indent, level);
  ret = ret && ds_strbuilder_append (sb,
                                     (const unsigned char *)"\"",
                                     xtree_node_name_get (node),
                                     (const unsigned char *)"\"",
                                     NULL);
  if (!ret) {
    ERROR (err, xtree_errcode_OOM, "OOM dumping key");
  }
  return ret;
}

static bool dump_object (struct xtree_errobj_t *err, const xtree_node_t *node,
                         ds_strbuilder_t *sb, size_t indent, size_t level)
{
  bool ret = true;
  const unsigned char *delim = (const unsigned char *)"";
  size_t nchildren = xtree_node_child_count (node);

  // ret = ret && dump_indent (err, sb, indent, level);
  ret = ret && ds_strbuilder_append (sb, (const unsigned char *)"{\n", NULL);

  for (size_t i=0; ret && i < nchildren; i++) {
    const xtree_node_t *node_field = xtree_node_child_get (err, node, i);
    if (!node_field) {
      ERROR (err, xtree_errcode_CHILD_NOT_FOUND, "Failed to find child %zu", i);
      return false;
    }
    const xtree_node_t *node_name = xtree_node_child_get (err, node_field, 0);
    const xtree_node_t *node_value = xtree_node_child_get (err, node_field, 1);
    ret = ret && ds_strbuilder_append (sb, delim, NULL);
    if (!(ret = ret && dump_key (err, node_name, sb, indent, level)) ||
        !(ret = ret && ds_strbuilder_append (sb, (const unsigned char *)": ", NULL)) ||
        !(ret = ret && tml_dump (err, node_value, sb, indent, level))) {
      return false;
    }
    delim = (const unsigned char *)",\n";
  }
  ret = ret && ds_strbuilder_append (sb, (const unsigned char *)"\n", NULL);
  ret = ret && dump_indent (err, sb, indent, level ? level - 1 : level);
  ret = ret && ds_strbuilder_append (sb, (const unsigned char *)"}", NULL);
  return ret;
}

static bool dump_array (struct xtree_errobj_t *err, const xtree_node_t *node,
                        ds_strbuilder_t *sb, size_t indent, size_t level)
{
  bool ret = true;
  const unsigned char *delim = (const unsigned char *)"\n";
  const xtree_node_t *child = NULL;
  size_t nchildren = xtree_node_child_count (node);

  ret = ret && ds_strbuilder_append (sb, (const unsigned char *)"[ ", NULL);

  for (size_t i=0; ret && i < nchildren; i++) {
    if (!(child = xtree_node_child_get (err, node, i))) {
      return false;
    }
    ret = ret && ds_strbuilder_append (sb, delim, NULL);
    ret = ret && dump_indent (err, sb, indent, level);
    ret = ret && tml_dump (err, child, sb, indent, level);
    delim = (const unsigned char *)",\n";
  }

  ret = ret && ds_strbuilder_append (sb, (const unsigned char *)"\n", NULL);
  ret = ret && dump_indent (err, sb, indent, level ? level - 1 : level);
  ret = ret && ds_strbuilder_append (sb, (const unsigned char *)"]", NULL);

  return ret;
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

static bool tml_dump (struct xtree_errobj_t *err, const xtree_node_t *node,
                       ds_strbuilder_t *sb, size_t indent, size_t level)
{
  /* Json-types are stored in the name field. We decide how to handle this using
   * that field:
   *   "array":        LIST
   *   "bool":         ATOM
   *   "number":       ATOM
   *   "object":       LIST
   *   "reference":    ATOM
   *   "string":       ATOM
   *   "symbol":       ATOM
   */
#define SH0(x)        (uint32_t)((x << 0))
#define SH8(x)        (uint32_t)((x << 8))
#define SH16(x)       (uint32_t)((x << 16))
#define HASH(a,b,c)       (uint32_t)((SH0(a) | SH8(b) | SH16(c)))

#define HARRAY      HASH('a', 'r', 'r')
#define HBOOL       HASH('b', 'o', 'o')
#define HNUMBER     HASH('n', 'u', 'm')
#define HOBJECT     HASH('o', 'b', 'j')
#define HREFERENCE  HASH('r', 'e', 'f')
#define HSTRING     HASH('s', 't', 'r')
#define HSYMBOL     HASH('s', 'y', 'm')

  const unsigned char *name = xtree_node_name_get (node);
  if (!name || (strlen ((const char *)name) < 3)) {
    ERROR (err, xtree_errcode_UNKNOWN,
           "Failed to retrieve name field from node %p", node);
    return false;
  }
  uint32_t hash = HASH(name[0], name[1], name[2]);
  bool ret = false;
  switch (hash) {
    case HARRAY:        ret = dump_array  (err, node, sb, indent, level + 1);  break;
    case HBOOL:         ret = dump_atom   (err, node, sb, indent, level + 1);  break;
    case HNUMBER:       ret = dump_atom   (err, node, sb, indent, level + 1);  break;
    case HOBJECT:       ret = dump_object (err, node, sb, indent, level + 1);  break;
    case HREFERENCE:    ret = dump_atom   (err, node, sb, indent, level + 1);  break;
    case HSTRING:       ret = dump_string (err, node, sb, indent, level + 1);  break;
    case HSYMBOL:       ret = dump_atom   (err, node, sb, indent, level + 1);  break;
    default:
      ERROR (err, xtree_errcode_UNKNOWN, "Hit default for node [%s]\n", name);
      break;
  }

#undef SH0
#undef SH8
#undef SH16
#undef HASH
#undef HARRAY
#undef HBOOL
#undef HNUMBER
#undef HOBJECT
#undef HREFERENCE
#undef HSTRING
#undef HSYMBOL

  return ret;
}

char *xtree_gtml_parse_dump (struct xtree_errobj_t *err,
                             const xtree_node_t *tml_node,
                             size_t indent)
{
  ds_strbuilder_t *sb = ds_strbuilder_new (NULL);
  if (!(tml_dump (err, tml_node, sb, indent, 0))) {
    ds_strbuilder_del (sb);
    return NULL;
  }
  char *ret = ds_strbuilder_build (sb);
  ds_strbuilder_del (sb);
  return ret;
}


