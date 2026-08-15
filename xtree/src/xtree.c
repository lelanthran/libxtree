#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "ds_str.h"
#include "ds_array.h"

#include "xtree.h"


static char *str_replace (char **dst, const char *src)
{
  if (!dst)
      return NULL;

  char *tmp = ds_str_dup (src);
  if (!tmp)
    return NULL;

  free (*dst);
  *dst = tmp;
  return *dst;
}


/* ***************************************************************************
 * Opaque type for key/value pairs.
 */
struct xtree_kv_t {
  char *name;
  char *value;
};

static void xtree_kv_free (xtree_kv_t **kv)
{
  if (!kv || !*kv)
    return;

  free ((*kv)->name);
  free ((*kv)->value);
  free (*kv);
}

static void xtree_kv_free_wrap (void *kv, void *ignore)
{
  (void)ignore;
  xtree_kv_free ((xtree_kv_t **)&kv);
}

static xtree_kv_t *xtree_kv_new (const char *name, const char *value)
{
  xtree_kv_t *ret = calloc (1, sizeof *ret);

  if (!ret ||
      !(ret->name = ds_str_dup (name)) ||
      !(ret->value = ds_str_dup (value))) {
    xtree_kv_free (&ret);
  }
  return ret;
}

static char *xtree_kv_value_set (xtree_kv_t *kv, const char *value)
{
  if (!kv)
      return NULL;

  return str_replace (&(kv->value), value);
}

static const char *xtree_kv_value_get (const xtree_kv_t *kv)
{
  return kv ? kv->value : NULL;
}

#if 0
static char *xtree_kv_name_set (xtree_kv_t *kv, const char *value)
{
  if (!kv)
      return NULL;

  return str_replace (&(kv->name), value);
}

static const char *xtree_kv_name_get (const xtree_kv_t *kv)
{
  return kv ? kv->name : NULL;
}
#endif


static xtree_kv_t *xtree_kvlist_find (ds_array_t *kvlist, const char *name)
{
  size_t nitems = ds_array_length (kvlist);

  for (size_t i=0; i<nitems; i++) {
    xtree_kv_t *kv = ds_array_get (kvlist, i);
    if ((strcmp (kv->name, name)) == 0) {
       return kv;
    }
  }
  return NULL;
}


















union atom_t {
  char *_value;           // Name of this node
  ds_array_t *_children;  // xtree_node_t pointers
};

struct xtree_node_t {
  enum xtree_node_type_t type;  // Type of this node (doesn't really make a difference

  xtree_node_t *parent;         // Parent node, or NULL if no parent

  ds_array_t *attrs;            // xtree_kv_t pointers

  union atom_t atom;
};



xtree_node_t *xtree_node_new (xtree_node_t *parent, enum xtree_node_type_t type)
{
  bool error = true;
  xtree_node_t *ret = calloc (1, sizeof *ret);

  if (!ret ||
      !(ret->attrs = ds_array_new ()))
    goto cleanup;

  ret->type = type;
  ret->parent = parent;

  switch (type) {
    case xtree_node_type_ATOM:
      if (!(ret->atom._value = ds_str_dup ("")))
        goto cleanup;
      break;

    case xtree_node_type_LIST:
      if (!(ret->atom._children = ds_array_new ()))
        goto cleanup;
      break;

    default:
      goto cleanup;
  }

  if (parent) {
    if (parent->type != xtree_node_type_LIST)
      goto cleanup;
    if (!(ds_array_ins_tail (parent->atom._children, ret)))
      goto cleanup;
  }

  error = false;
cleanup:
  if (error)
    xtree_node_free (&ret);

  return ret;
}


void xtree_node_free (xtree_node_t **node)
{
  if (!node || !*node)
    return;

  xtree_node_t *n = *node;
  xtree_node_t *parent = n->parent;

  ds_array_iterate (n->attrs, xtree_kv_free_wrap, NULL);
  ds_array_del (n->attrs);

  size_t nchildren = 0;

  switch (n->type) {
    case xtree_node_type_ATOM:
      free (n->atom._value);
      break;

    case xtree_node_type_LIST:
      nchildren = ds_array_length (n->atom._children);
      for (size_t i=nchildren + 1; i > 0; i--) {
        xtree_node_t *child = ds_array_get (n->atom._children, i-1);
        xtree_node_free (&child);
      }
      ds_array_del (n->atom._children);
      break;

    default:
      break;
  }

  // Remove current node from parent->children array
  if (parent)
    ds_array_rm_ptr (parent->atom._children, n);

  free (n);
  *node = NULL;
}

void xtree_node_dump (const xtree_node_t *node, FILE *outf, size_t depth)
{
  if (!outf)
    outf = stdout;
  if (!node) {
    fprintf (outf, "NULL node_t object\n");
    return;
  }

#define INDENT    for (size_t i=0; i<depth; i++) fprintf (outf, " ")

  INDENT;
  fprintf (outf, "node:%p\n", node);
  INDENT;
  fprintf (outf, "node:type     [%i]\n", node->type);
  INDENT;
  fprintf (outf, "node:parent   [%p]\n", node->parent);
  INDENT;
  fprintf (outf, "node:nattrs     %zu\n", ds_array_length (node->attrs));
  size_t nitems = ds_array_length (node->attrs);
  for (size_t i=0; i<nitems; i++) {
    xtree_kv_t *kv = ds_array_get (node->attrs, i);
    INDENT;
    fprintf (outf, "node:%p:attr  [%s:%s]\n",
             node, kv->name, kv->value);
  }

  INDENT;

  switch (node->type) {
    case xtree_node_type_ATOM:
      fprintf (outf, "node:value  [%s]\n", node->atom._value);
      break;

    case xtree_node_type_LIST:
      fprintf (outf, "node:nchildren  %zu\n", ds_array_length (node->atom._children));
      nitems = ds_array_length (node->atom._children);

      for (size_t i=0; i<nitems; i++) {
        xtree_node_t *child = ds_array_get (node->atom._children, i);
        xtree_node_dump (child, outf, (depth + 1));
      }
      break;
  }

#undef INDENT
}

const char *xtree_node_value_set (xtree_node_t *node, const char *value)
{
  if (!node)
    return false;
  return str_replace (&node->atom._value, value);
}

const char *xtree_node_value_get (xtree_node_t *node)
{
  if (!node || node->type != xtree_node_type_ATOM)
    return NULL;

  return node->atom._value;
}





const char *xtree_node_attr_new (xtree_node_t *node, const char *name, const char *value)
{
  bool error = true;
  xtree_kv_t *tmp = xtree_kv_new (name, value);
  if (!tmp)
    goto cleanup;

  if (!(ds_array_ins_tail (node->attrs, tmp)))
    goto cleanup;

  error = false;
cleanup:
  if (error)
    xtree_kv_free (&tmp);

  return (!error) ? tmp->value : NULL;
}


const char *xtree_node_attr_value_set (xtree_node_t *node, const char *name, const char *value)
{
  if (!node || !name)
    return NULL;

  xtree_kv_t *found = xtree_kvlist_find (node->attrs, name);
  return found ? xtree_kv_value_set (found, value) : NULL;
}


const char *xtree_node_attr_value_get1 (xtree_node_t *node, const char *name)
{
  if (!node || !name)
    return NULL;

  xtree_kv_t *found = xtree_kvlist_find (node->attrs, name);
  return found ? xtree_kv_value_get (found) : NULL;
}



