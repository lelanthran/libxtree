#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "ds_str.h"
#include "ds_array.h"

#include "xtree_err.h"
#include "xtree.h"


static char *str_replace (struct xtree_errobj_t *err, char **dst, const char *src)
{
  if (!dst)
      return NULL;

  char *tmp = ds_str_dup (src);
  if (!tmp) {
    ERROR (err, xtree_errcode_OOM, "OOM allocating new string [%s]\n", src);
    return NULL;
  }

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

static char *xtree_kv_value_set (struct xtree_errobj_t *err, xtree_kv_t *kv, const char *value)
{
  if (!kv) {
    ERROR (err, xtree_errcode_PARAMETER_TYPE, "KV object cannot be NULL.");
    return NULL;
  }

  return str_replace (err, &(kv->value), value);
}

static const char *xtree_kv_value_get (struct xtree_errobj_t *err,
                                       const xtree_kv_t *kv)
{
  if (!kv) {
    ERROR (err, xtree_errcode_PARAMETER_TYPE, "KV object is NULL");
    return NULL;
  }
  return kv->value;
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

















const char *xtree_node_type_string (enum xtree_node_type_t type)
{
  static char unknown[64];
#define TYPE(x)  { x, #x }
  static const struct {
    enum xtree_node_type_t type;
    const char *s;
  } types[] = {
    TYPE (xtree_node_type_UNKNOWN),
    TYPE (xtree_node_type_ATOM),
    TYPE (xtree_node_type_LIST),
  };
#undef TYPE
  static const size_t ntypes = sizeof types / sizeof types[0];

  for (size_t i=0; i < ntypes; i++) {
    if (types[i].type == type)
      return types[i].s;
  }
  snprintf (unknown, sizeof unknown, "%i: unknown node type", type);
  return unknown;
}
















union atom_t {
  char *_value;           // Name of this node
  ds_array_t *_children;  // xtree_node_t pointers
};

struct xtree_node_t {
  enum xtree_node_type_t type;  // Type of this node

  char *name;                   // Optional name for this node
  xtree_node_t *parent;         // Parent node, or NULL if no parent
  ds_array_t *attrs;            // xtree_kv_t pointers
  union atom_t atom;            // Node data or an array of children
};


// Helper functions to enforce policies for parameters (node is a list, etc).
static bool check_type (struct xtree_errobj_t *err,
                        const xtree_node_t *node,
                        enum xtree_node_type_t type)
{
  xtree_errobj_clrerr (err);
  if (!node || node->type != type) {
    ERROR (err, xtree_errcode_PARAMETER_TYPE,
           "Expected note type [%s], got [%s]",
           xtree_node_type_string (type),
           xtree_node_type_string (node->type));
    return false;
  }
  return true;
}

static bool check_list (struct xtree_errobj_t *err,
                        const xtree_node_t *node)
{
  return check_type (err, node, xtree_node_type_LIST);
}

#if 0 // TODO add a call to this for any function that has to be typed as an
      // atom
static bool check_atom (struct xtree_errobj_t *err,
                        const xtree_node_t *node)
{
  return check_type (err, node, xtree_node_type_ATOM);
}
#endif

static bool check_bounds (struct xtree_errobj_t *err,
                          const xtree_node_t *node, size_t position)
{
  xtree_errobj_clrerr (err);
  if (position >= ds_array_length (node->atom._children)) {
    ERROR (err, xtree_errcode_OUT_OF_BOUNDS,
              "Requested [%zu] child of [%zu] children",
              position, ds_array_length (node->atom._children));
    return false;
  }
  return true;
}







xtree_node_t *xtree_node_new (struct xtree_errobj_t *err,
                              xtree_node_t *parent,
                              const char *optional_name,
                              enum xtree_node_type_t type)
{
  bool error = true;
  xtree_node_t *ret = calloc (1, sizeof *ret);

  xtree_errobj_clrerr (err);

  if (!optional_name)
    optional_name = "";

  if (!ret ||
      !(ret->name = ds_str_dup (optional_name)) ||
      !(ret->attrs = ds_array_new ())) {
    ERROR (err, xtree_errcode_OOM, "Failed to create node object.");
    goto cleanup;
  }

  ret->type = type;

  switch (type) {
    case xtree_node_type_ATOM:
      if (!(ret->atom._value = ds_str_dup (""))) {
        ERROR (err, xtree_errcode_OOM, "Failed to create atom.");
        goto cleanup;
      }
      break;

    case xtree_node_type_LIST:
      if (!(ret->atom._children = ds_array_new ())) {
        ERROR (err, xtree_errcode_OOM, "Failed to create list.");
        goto cleanup;
      }
      break;

    default:
      ERROR (err, xtree_errcode_PARAMETER_TYPE, "Bad parameter.");
      goto cleanup;
  }

  if (parent) {
    if (parent->type != xtree_node_type_LIST) {
      ERROR (err, xtree_errcode_PARAMETER_TYPE, "Parent is not a LIST.");
      goto cleanup;
    }
    if (!(ds_array_ins_tail (parent->atom._children, ret))) {
      ERROR (err, xtree_errcode_OOM, "Failed to attach node to parent.");
      goto cleanup;
    }
    ret->parent = parent;
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

  free (n->name);

  ds_array_iterate (n->attrs, xtree_kv_free_wrap, NULL);
  ds_array_del (n->attrs);

  size_t nchildren = 0;

  switch (n->type) {
    case xtree_node_type_ATOM:
      free (n->atom._value);
      break;

    case xtree_node_type_LIST:
      nchildren = ds_array_length (n->atom._children);
      for (size_t i=nchildren; i > 0; i--) {
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
  fprintf (outf, "node:%p (%s)\n", node, node->name);
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

    case xtree_node_type_UNKNOWN:
    default:
      // Do nothing
      break;
  }

#undef INDENT
}

const char *xtree_node_value_set (struct xtree_errobj_t *err,
                                  xtree_node_t *node, const char *value)
{
  xtree_errobj_clrerr (err);

  if (!node || node->type != xtree_node_type_ATOM) {
    ERROR (err, xtree_errcode_PARAMETER_TYPE, "Node is not an ATOM.");
    return NULL;
  }
  return str_replace (err, &node->atom._value, value);
}

const char *xtree_node_value_get (struct xtree_errobj_t *err,
                                  const xtree_node_t *node)
{
  xtree_errobj_clrerr (err);

  if (!node || node->type != xtree_node_type_ATOM) {
    ERROR (err, xtree_errcode_PARAMETER_TYPE, "Node is not an ATOM.");
    return NULL;
  }

  return node->atom._value;
}

size_t xtree_node_child_count (const xtree_node_t *node)
{
  return
    (node && node->type == xtree_node_type_LIST)
    ? ds_array_length (node->atom._children)
    : 0;
}

xtree_node_t *xtree_node_child_get (struct xtree_errobj_t *err,
                                    const xtree_node_t *node, size_t position)
{
  if (!(check_list (err, node)) || !(check_bounds (err, node, position)))
    return NULL;

  return ds_array_get (node->atom._children, position);
}

size_t xtree_node_child_find (struct xtree_errobj_t *err,
                              const xtree_node_t *parent,
                              const xtree_node_t *child)
{
  if (!(check_list (err, parent)))
    return (size_t)-1;

  if (!child) {
    ERROR (err, xtree_errcode_PARAMETER_TYPE,
           "Cannot search for a child pointer of value NULL");
    return (size_t)-1;
  }

  size_t pos = ds_array_ptr_index (parent->atom._children, child);
  if (pos == (size_t)-1) {
    ERROR (err, xtree_errcode_CHILD_NOT_FOUND,
           "Child %p is not found in parent %p", child, parent);
    return (size_t)-1;
  }
  return pos;
}

xtree_node_t *xtree_node_child_append (struct xtree_errobj_t *err,
                                       xtree_node_t *parent,
                                       xtree_node_t *child)
{
  if (!(check_list (err, parent)))
    return NULL;

  if (child->parent) {
    size_t pos = xtree_node_child_find (err, child->parent, child);
    if (pos == (size_t) -1) {
      ERROR (err, xtree_errcode_INTERNAL_STATE,
             "Child %p not found in child->parent %p\n",
             child, child->parent);
      return NULL;
    }
    xtree_node_t *tmp = xtree_node_child_detach (err, child->parent, pos);
    if (!tmp)
      return NULL;
  }

  if (!(ds_array_ins_tail (parent->atom._children, child))) {
    ERROR (err, xtree_errcode_OOM,
           "Failed to append child %p to parent %p",
           child, parent);
    return NULL;
  }

  child->parent = parent;
  return child;
}

xtree_node_t *xtree_node_child_detach (struct xtree_errobj_t *err,
                                       xtree_node_t *node, size_t position)
{
  if (!(check_list (err, node)) || !(check_bounds (err, node, position)))
    return NULL;

  xtree_node_t *child = ds_array_get (node->atom._children, position);
  // TODO: Maybe switch to a linked list so existing references are not
  // todo: invalidated? Helps in a subset of cases, but not when the actual
  // todo: node has been deleted. No ownership enforcement as in Rust, but
  // todo: even if we have them we probably would have to use unsafe for
  // todo: back-references anyway.
  ds_array_rm_ptr (node->atom._children, child);
  child->parent = NULL;
  return child;
}

xtree_node_t *xtree_node_child_attach (struct xtree_errobj_t *err,
                                       xtree_node_t *parent,
                                       xtree_node_t *child,
                                       size_t position)
{
  if (!(check_list (err, parent)))
    return NULL;

  if (!child) {
    ERROR (err, xtree_errcode_PARAMETER_TYPE, "child node is NULL");
    return NULL;
  }

  // If position is out of bound, simply append and return
  if (position >= ds_array_length (parent->atom._children)) {
    return xtree_node_child_append (err, parent, child);
  }

  // Detach child from existing parent
  if (child->parent) {
    size_t pos = xtree_node_child_find (err, child->parent, child);
    if (pos == (size_t) -1) {
      ERROR (err, xtree_errcode_INTERNAL_STATE,
             "Child %p not found in child->parent %p\n",
             child, child->parent);
      return NULL;
    }
    xtree_node_t *tmp = xtree_node_child_detach (err, child->parent, pos);
    if (!tmp)
      return NULL;
  }

  if (!(ds_array_ins (parent->atom._children, child, position))) {
    ERROR (err, xtree_errcode_OOM,
           "Failed to insert child %p into parent %p at positions %zu\n",
           child, parent, position);
    return NULL;
  }
  child->parent = parent;
  return child;
}

const char *xtree_node_attr_new (struct xtree_errobj_t *err,
                                 xtree_node_t *node, const char *name, const char *value)
{
  bool error = true;
  xtree_kv_t *tmp = xtree_kv_new (name, value);

  xtree_errobj_clrerr (err);

  if (!tmp) {
    ERROR (err, xtree_errcode_OOM, "OOM creating kv object.");
    goto cleanup;
  }

  if (!(ds_array_ins_tail (node->attrs, tmp))) {
    ERROR (err, xtree_errcode_OOM, "OOM attaching KV object.");
    goto cleanup;
  }

  error = false;
cleanup:
  if (error)
    xtree_kv_free (&tmp);

  return (!error) ? tmp->value : NULL;
}


const char *xtree_node_attr_value_set (struct xtree_errobj_t *err,
                                       xtree_node_t *node, const char *name, const char *value)
{
  xtree_errobj_clrerr (err);

  if (!node || !name) {
    ERROR (err, xtree_errcode_PARAMETER_TYPE,
           "Node and name must be non-null [%p/%p].", node, name);
    return NULL;
  }

  xtree_kv_t *found = xtree_kvlist_find (node->attrs, name);
  if (!found) {
    ERROR (err, xtree_errcode_ATTRIBUTE_NOT_FOUND,
           "[%s]: attribute not found", name);
    return NULL;
  }
  return xtree_kv_value_set (err, found, value);
}


const char *xtree_node_attr_value_get1 (struct xtree_errobj_t *err,
                                        xtree_node_t *node, const char *name)
{
  xtree_errobj_clrerr (err);

  if (!node || !name) {
    ERROR (err, xtree_errcode_PARAMETER_TYPE,
           "Node or name is NULL [%p:%p]", node, name);
    return NULL;
  }

  xtree_kv_t *found = xtree_kvlist_find (node->attrs, name);
  if (!found) {
    ERROR (err, xtree_errcode_ATTRIBUTE_NOT_FOUND,
           "[%s]: Attribute not found", name);
    return NULL;
  }
  return xtree_kv_value_get (err, found);
}



