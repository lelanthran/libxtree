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
















/* ****************************************************************************
 * Nodes have type (atom/list), name (optional) and value { text | children }.
 *
 */


static xtree_node_t *toplevel_parse (struct xtree_errobj_t *err,
                                     xtree_node_t *parent,
                                     xtree_reader_t *reader,
                                     size_t depth,
                                     size_t max_depth)
{
  (void)parent;
  (void)reader;
  (void)depth;
  (void)max_depth;
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

    /* ********************************************************************** */

#define ATOM(jtype,token) \
  xtree_node_new_atom (err,\
                       parent,\
                       (const unsigned char *)jtype,\
                       xtree_scan_token_text (token), \
                       NULL);

    default:
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


