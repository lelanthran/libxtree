#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "ds_str.h"
#include "ds_array.h"

#include "xtree_err.h"
#include "xtree.h"


static unsigned char *str_replace (struct xtree_errobj_t *err,
                                   unsigned char **dst, const unsigned char *src)
{
  if (!dst)
      return NULL;

  if (!src)
    src = (unsigned char *)"";

  unsigned char *tmp = (unsigned char *)ds_str_dup ((char *)src);
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
  unsigned char *name;
  unsigned char *value;
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

static xtree_kv_t *xtree_kv_new (const unsigned char *name, const unsigned char *value)
{
  xtree_kv_t *ret = calloc (1, sizeof *ret);

  if (!ret ||
      !(ret->name = (unsigned char *)ds_str_dup ((char *)name)) ||
      !(ret->value = (unsigned char *)ds_str_dup ((char *)value))) {
    xtree_kv_free (&ret);
  }
  return ret;
}

static unsigned char *xtree_kv_value_set (struct xtree_errobj_t *err,
                                          xtree_kv_t *kv, const unsigned char *value)
{
  if (!kv) {
    ERROR (err, xtree_errcode_PARAMETER_TYPE, "KV object cannot be NULL.");
    return NULL;
  }

  return str_replace (err, &(kv->value), value);
}

static const unsigned char *xtree_kv_value_get (struct xtree_errobj_t *err,
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


static xtree_kv_t *xtree_kvlist_find (ds_array_t *kvlist, const unsigned char *name)
{
  size_t nitems = ds_array_length (kvlist);

  for (size_t i=0; i<nitems; i++) {
    xtree_kv_t *kv = ds_array_get (kvlist, i);
    if ((strcmp ((char *)kv->name, (char *)name)) == 0) {
       return kv;
    }
  }
  return NULL;
}

static xtree_kv_t *xtree_kvlist_get (ds_array_t *kvlist,
                                     const unsigned char *name, size_t n)
{
  size_t nitems = ds_array_length (kvlist);
  size_t count = 0;
  for (size_t i=0; i < nitems; i++) {
    xtree_kv_t *kv = ds_array_get (kvlist, i);
    if (((strcmp ((char *)kv->name, (char *)name)) == 0)) {
      if (count == n)
        return kv;
      count++;
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
  unsigned char *_value;           // value of this node
  ds_array_t *_children;  // xtree_node_t pointers
};

struct xtree_node_t {
  enum xtree_node_type_t type;  // Type of this node

  unsigned char *name;          // Optional name for this node
  xtree_node_t *parent;         // Parent node, or NULL if no parent
  ds_array_t *attrs;            // xtree_kv_t pointers
  union atom_t atom;            // Node data or an array of children
};


// Helper functions to enforce policies for parameters (node is a list, etc).

static bool check_nullparam (struct xtree_errobj_t *err,
                             const void *p,
                             const char *name)
{
  if (!p) {
    ERROR (err, xtree_errcode_NULL_PARAMETER, "%s: NULL param specified", name);
    return false;
  }
  return true;
}

static bool check_type (struct xtree_errobj_t *err,
                        const xtree_node_t *node,
                        enum xtree_node_type_t type)
{
  xtree_errobj_reset (err, 1024);
  if (!node || node->type != type) {
    ERROR (err, xtree_errcode_PARAMETER_TYPE,
           "Expected note type [%s], got [%s]",
           xtree_node_type_string (type),
           node ? xtree_node_type_string (node->type) : "NULL node object");
    return false;
  }
  return true;
}

static bool check_list (struct xtree_errobj_t *err,
                        const xtree_node_t *node)
{
  return check_type (err, node, xtree_node_type_LIST);
}

static bool check_atom (struct xtree_errobj_t *err,
                        const xtree_node_t *node)
{
  return check_type (err, node, xtree_node_type_ATOM);
}

static bool check_bounds (struct xtree_errobj_t *err,
                          const xtree_node_t *node, size_t position)
{
  xtree_errobj_reset (err, 1024);
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
                              const unsigned char *optional_name,
                              enum xtree_node_type_t type)
{
  bool error = true;
  xtree_node_t *ret = calloc (1, sizeof *ret);

  xtree_errobj_reset (err, 1024);

  if (!optional_name)
    optional_name = (const unsigned char *)"";

  if (!ret ||
      !(ret->name = (unsigned char *)ds_str_dup ((char *)optional_name)) ||
      !(ret->attrs = ds_array_new ())) {
    ERROR (err, xtree_errcode_OOM, "Failed to create node object.");
    goto cleanup;
  }

  ret->type = type;

  switch (type) {
    case xtree_node_type_ATOM:
      if (!(ret->atom._value = (unsigned char *)ds_str_dup (""))) {
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

xtree_node_t *xtree_node_new_atom (struct xtree_errobj_t *err,
                                   xtree_node_t *parent,
                                   const unsigned char *optional_name,
                                   const unsigned char *value, ...)
{
  va_list ap;
  va_start (ap, value);
  xtree_node_t *ret = xtree_node_new_atomv (err, parent, optional_name, value, ap);
  va_end (ap);
  return ret;
}

xtree_node_t *xtree_node_new_atomv (struct xtree_errobj_t *err,
                                    xtree_node_t *parent,
                                    const unsigned char *optional_name,
                                    const unsigned char *value, va_list ap)
{
  bool error = true;

  xtree_node_t *ret = NULL;
  unsigned char *tmp = NULL;

  if (!(tmp = (unsigned char *)ds_str_vcat ((char *)value, ap))) {
    ERROR (err, xtree_errcode_OOM, "OOM");
    goto cleanup;
  }
  if (!(ret = xtree_node_new (err, parent, optional_name, xtree_node_type_ATOM))) {
    goto cleanup;
  }
  free (ret->atom._value);
  ret->atom._value = tmp;
  tmp = NULL;

  error = false;

cleanup:
  free (tmp);
  if (error) {
    xtree_node_free (&ret);
  }
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

void xtree_node_dump_file (const xtree_node_t *node, FILE *outf, size_t depth)
{
  if (!outf)
    outf = stdout;
  if (!node) {
    fprintf (outf, "NULL node_t object\n");
    return;
  }

#define INDENT    for (size_t i=0; i<depth; i++) fprintf (outf, " ")

  INDENT;
  fprintf (outf, "node:         (%s)\n", node->name);
  INDENT;
  fprintf (outf, "node:type     [%i]\n", node->type);
  INDENT;
  fprintf (outf, "node:parent   [%s]\n",
           node->parent ? (char *)node->parent->name : "No parent");
  INDENT;
  fprintf (outf, "node:nattrs     %zu\n", ds_array_length (node->attrs));
  size_t nitems = ds_array_length (node->attrs);
  for (size_t i=0; i<nitems; i++) {
    xtree_kv_t *kv = ds_array_get (node->attrs, i);
    INDENT;
    fprintf (outf, "node:%s:attr[%s:%s]\n",
             node->name, kv->name, kv->value);
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
        xtree_node_dump_file (child, outf, (depth + 1));
      }
      break;

    case xtree_node_type_UNKNOWN:
    default:
      // Do nothing
      break;
  }

#undef INDENT
}


static char *sprintv (char **dst, const char *fmt, va_list ap)
{
  va_list copy;
  va_copy (copy, ap);
  int add_len = vsnprintf (NULL, 0, fmt, copy);
  va_end (copy);
  char *tmp = malloc ((size_t)add_len + 1);
  if (!tmp) {
    return NULL;
  }

  vsnprintf (tmp, (size_t)add_len + 1, fmt, ap);

  if (!(ds_str_append (dst, tmp, NULL))) {
    free (tmp);
    return NULL;
  }

  free (tmp);
  return *dst;
}

static char *sprint (char **dst, const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  char *ret = sprintv (dst, fmt, ap);
  va_end (ap);
  return ret;
}

static char *dump_str (const xtree_node_t *node, char **dst, size_t depth)
{
  if (!node) {
    sprint (dst, "NULL node_t object\n");
    return *dst;
  }

#define INDENT    for (size_t i=0; i<depth; i++) sprint (dst, " ")

  INDENT;
  sprint (dst, "node:         (%s)\n", node->name);
  INDENT;
  sprint (dst, "node:type     [%i]\n", node->type);
  INDENT;
  sprint (dst, "node:parent   [%s]\n",
          node->parent ? (char *)node->parent->name : "No parent");
  INDENT;
  sprint (dst, "node:nattrs     %zu\n", ds_array_length (node->attrs));
  size_t nitems = ds_array_length (node->attrs);
  for (size_t i=0; i<nitems; i++) {
    xtree_kv_t *kv = ds_array_get (node->attrs, i);
    INDENT;
    sprint (dst, "node:%s:attr[%s:%s]\n",
             node->name, kv->name, kv->value);
  }

  INDENT;

  switch (node->type) {
    case xtree_node_type_ATOM:
      sprint (dst, "node:value  [%s]\n", node->atom._value);
      break;

    case xtree_node_type_LIST:
      sprint (dst, "node:nchildren  %zu\n", ds_array_length (node->atom._children));
      nitems = ds_array_length (node->atom._children);

      for (size_t i=0; i<nitems; i++) {
        xtree_node_t *child = ds_array_get (node->atom._children, i);
        dump_str (child, dst, (depth + 1));
      }
      break;

    case xtree_node_type_UNKNOWN:
    default:
      // Do nothing
      break;
  }

#undef INDENT
  return *dst;
}

char *xtree_node_dump_str (const xtree_node_t *node, size_t depth)
{
  char *dst = NULL;
  return dump_str (node, &dst, depth);
}

const unsigned char *xtree_node_name_get (const xtree_node_t *node)
{
  return (node && node->name) ? node->name : (unsigned char *)"";
}

const unsigned char *xtree_node_name_set (struct xtree_errobj_t *err,
                                 xtree_node_t *node, const unsigned char *name)
{
  xtree_errobj_reset (err, 1024);
  if (!(check_nullparam (err, node, "node")))
    return NULL;
  return str_replace (err, &node->name, name);
}

enum xtree_node_type_t xtree_node_type_get (const xtree_node_t *node)
{
  return node ? node->type : xtree_node_type_UNKNOWN;
}

xtree_node_t *xtree_node_parent (struct xtree_errobj_t *err,
                                 const xtree_node_t *node)
{
  xtree_errobj_reset (err, 1024);
  if (!(check_nullparam (err, node, "node")))
    return NULL;
  return node->parent;
}

const unsigned char *xtree_node_value_get (struct xtree_errobj_t *err,
                                           const xtree_node_t *node)
{
  if (!(check_atom (err, node)))
    return NULL;

  return node->atom._value;
}

const unsigned char *xtree_node_value_set (struct xtree_errobj_t *err,
                                           xtree_node_t *node,
                                           const unsigned char *value)
{
  if (!(check_atom (err, node)))
    return NULL;

  return str_replace (err, &node->atom._value, value);
}

const unsigned char *xtree_node_value_append (struct xtree_errobj_t *err,
                                              xtree_node_t *node,
                                              const unsigned char *extra)
{
  if (!(check_atom (err, node)))
    return NULL;

  unsigned char *tmp = (unsigned char *)ds_str_cat (node->atom._value ?
                                                      (char *)node->atom._value :
                                                      "",
                                                    extra, NULL);
  if (!tmp) {
    ERROR (err, xtree_errcode_OOM,
           "OOM appending value [%s] + [%s]",
           node->name, extra);
    return NULL;
  }
  free (node->atom._value);
  node->atom._value = tmp;
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
  if (!(check_list (err, parent)) || !(check_nullparam (err, child, "child")))
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












const unsigned char *xtree_node_attr_new (struct xtree_errobj_t *err,
                                          xtree_node_t *node,
                                          const unsigned char *name,
                                          const unsigned char *value)
{
  bool error = true;
  xtree_kv_t *tmp = NULL;

  if (!(check_nullparam (err, node, "node")) ||
      !(check_nullparam (err, name, "name")) ||
      !(check_nullparam (err, value, "value")))
    return NULL;

  if (!(tmp = xtree_kv_new (name, value))) {
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

size_t xtree_node_attr_count (const xtree_node_t *node)
{
  return node ? ds_array_length (node->attrs) : 0;
}

const unsigned char *xtree_node_attr_value_set (struct xtree_errobj_t *err,
                                                xtree_node_t *node,
                                                const unsigned char *name,
                                                const unsigned char *value)
{
  if (!(check_nullparam (err, node, "node")) ||
      !(check_nullparam (err, name, "name")) ||
      !(check_nullparam (err, value, "value")))
    return NULL;

  xtree_kv_t *found = xtree_kvlist_find (node->attrs, name);
  if (!found) {
    ERROR (err, xtree_errcode_ATTRIBUTE_NOT_FOUND,
           "[%s]: attribute not found", name);
    return NULL;
  }
  return xtree_kv_value_set (err, found, value);
}

const unsigned char *xtree_node_attr_value_get (struct xtree_errobj_t *err,
                                                const xtree_node_t *node,
                                                const unsigned char *name, size_t n)
{
  if (!(check_nullparam (err, node, "node")) ||
      !(check_nullparam (err, name, "name")))
    return NULL;

  const xtree_kv_t *kv = xtree_kvlist_get (node->attrs, name, n);
  if (!kv) {
    ERROR (err, xtree_errcode_OUT_OF_BOUNDS, "[%zu]: Out of bounds for kv list", n);
    return NULL;
  }
  return kv->value;
}

const unsigned char *xtree_node_attr_value_get1 (struct xtree_errobj_t *err,
                                                 const xtree_node_t *node,
                                                 const unsigned char *name)
{
  xtree_errobj_reset (err, 1024);

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



