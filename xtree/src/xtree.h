
#ifndef H_XTREE
#define H_XTREE


/* ***************************************************************************
 * An xtree is a generic tree suitable for storing XML, HTML or similar trees.
 * It can also store JSON trees, s-expressions, etc. It is intended to be an
 * internal interchange format between various tree representations.
 *
 * In the same spirit as CORBA, COM, etc, the intention is to have a single
 * internal format that any adaptor can serialise to and from, so that a tree
 * ingested in any adaptor's format can be outputted in any other adaptors
 * format.
 *
 * The general form is:
 * Each node stores a list of one or more atoms, in addition to a pointer to a
 * parent node and an ordered list (array) of children nodes. While this general
 * structure can represent everything, for convenience each node also stores a
 * name and a set of KV attributes (both plain text).
 *
 * A general tree such as this is applicable to a really large variety of
 * uses:
 * 1. Storing HTML (or XML type tagged trees)
 * 2. Storing JSON
 * 3. Storing s-expressions.
 * 4. Storing Markdown
 * 5. Storing program code (as an AST).
 *
 * In general, using a general tree datastructure is helpful for a much larger
 * set of problems that one may initially realise.
 *
 */

typedef struct xtree_kv_t xtree_kv_t;
typedef struct xtree_node_t xtree_node_t;

enum xtree_node_type_t {
  xtree_node_type_UNKNOWN = 0,
  xtree_node_type_ATOM,
  xtree_node_type_LIST,
};

#ifdef __cplusplus
extern "C" {
#endif

// Turn a node type enum into a string, useful doe diagnostics. Caller must
// not free or mutate the returned value.
const char *xtree_node_type_string (enum xtree_node_type_t type);

// Create a new node using the specified node as a parent. If no parent is
// specified then the returned node is a root node. On error NULL is returned
// and extended error information is stored in the mandatory `err` parameter.
// Only allowable types are `_ATOM` and `_LIST`.
xtree_node_t *xtree_node_new (struct xtree_errobj_t *err,
                              xtree_node_t *parent,
                              const char *optional_name,
                              enum xtree_node_type_t type);

// Free the specified node, removing it from any parent if necessary. Also
// sets the pointer to NULL so callers can call this multiple times with the
// same variable. Parameter `node` itself can be NULL, too.
void xtree_node_free (xtree_node_t **node);

// Dump the node recursively. Used only during testing. If `outf` is NULL,
// stdout is used.
void xtree_node_dump (const xtree_node_t *node, FILE *outf, size_t depth);

// Set/get the name of the node. Note that the node name is optional, and if not
// set, or set to NULL, will default to `""`. The caller must not free or mutate
// the returned value. NULL is returned on error and extended error information
// is available in parameter `err` for `_name_set`.
const char *xtree_node_name_get (const xtree_node_t *node);
const char *xtree_node_name_set (struct xtree_errobj_t *err,
                                 xtree_node_t *node, const char *name);

// Return the type of the node. This can only be set during node creation. On
// success the node type is returned, on error _UNKNOWN is returned.
enum xtree_node_type_t xtree_node_type_get (const xtree_node_t *node);

// Return the parent of the node, or NULL on error. Extended error information
// is available in the mandatory parameter `err`.
xtree_node_t *xtree_node_parent (struct xtree_errobj_t *err,
                                 const xtree_node_t *node);

// Set/get the value of a node. Only applies to nodes of type _ATOM, returns
// error for all other nodes with extended error information in the mandatory
// `err` parameter. The caller must not free or mutate the returned value.
const char *xtree_node_value_get (struct xtree_errobj_t *err,
                                  const xtree_node_t *node);
// Returns the node's value on success.
const char *xtree_node_value_set (struct xtree_errobj_t *err,
                                  xtree_node_t *node,
                                  const char *value);
// Returns the node's value on success.
const char *xtree_node_value_append (struct xtree_errobj_t *err,
                                     xtree_node_t *node,
                                     const char *extra);









// Return the count of child nodes in a node of type LIST. On success the
// count is returned, on error (the node is NULL or not a LIST), `0` is
// returned.
size_t xtree_node_child_count (const xtree_node_t *node);

// Return a pointer to the i'th child from the node specified. Caller must not
// free the returned value, although all other node operations are allowed on
// it (value, attribute mutation, etc). On error NULL is returned and extended
// error information is stored in the mandatory `err` parameter.
xtree_node_t *xtree_node_child_get (struct xtree_errobj_t *err,
                                    const xtree_node_t *node, size_t position);

// Returns the position of the specified child node within the specified
// parent node. On success returns the zero-based position, on failure returns
// `(size_t)-1` and extended error information is stored in the `err`
// parameter.
// NOTE: Missing test
size_t xtree_node_child_find (struct xtree_errobj_t *err,
                              const xtree_node_t *parent,
                              const xtree_node_t *child);

// Append a child node to a parent node. Both nodes have to be sensible:
// parent has to be non-NULL and of type LIST and child has to be non-null.
// On success a pointer to the child node is returned. On error NULL is
// returned and extended error information is stored in the `err` parameter.
xtree_node_t *xtree_node_child_append (struct xtree_errobj_t *err,
                                       xtree_node_t *parent,
                                       xtree_node_t *child);

// Remove the i'th child from the node specified, and return it. Caller is
// responsible for freeing the returned node, using `xtree_node_free()`. Note
// that the returned node is, by definition, a root node as it is not attached
// to any parent. The child node is returned on success. On error NULL is
// returned and extended error information is stored in the mandatory `err`
// parameter.
xtree_node_t *xtree_node_child_detach (struct xtree_errobj_t *err,
                                       xtree_node_t *node, size_t position);

// Attach the specified child node to the specified parent node in the
// specified position. If the position is out of bounds, the child is appended
// to the parent node. The child node is detached from the existing parent
// node, if any. The child node is returned on success. On error NULL is
// returned and extended error information is stored in the mandatory `err`
// parameter.
xtree_node_t *xtree_node_child_attach (struct xtree_errobj_t *err,
                                       xtree_node_t *parent,
                                       xtree_node_t *child,
                                       size_t position);








// Add an attribute to a node. In the event that the attribute with the
// specified name already exists, a new attribute with the same name is
// created and stored. On success a pointer to the value of the new attribute
// is returned. On error NULL is returned an error information is stored in
// the mandatory `err` parameter.
const char *xtree_node_attr_new (struct xtree_errobj_t *err,
                                 xtree_node_t *node, const char *name, const char *value);

// Returns the number of attributes in the node on success. If the node is
// NULL or the attribute does not exist, `0` is returned.
// error.
size_t xtree_node_attr_count (const xtree_node_t *node);

// Set the first attribute with the specified name to the specified value, or
// return NULL if the attribute does not exist with extended error information
// stored in the mandatory `err` parameter.
const char *xtree_node_attr_value_set (struct xtree_errobj_t *err,
                                       xtree_node_t *node, const char *name, const char *value);

// Return the n'th attribute of the specified name, or NULL if the attribute
// does not exist or if an error occurred. Error information is stored in the
// mandatory `err` parameter.
const char *xtree_node_attr_value_get (struct xtree_errobj_t *err,
                                       xtree_node_t *node, size_t i);

// Return the first attribute of the specified name, or NULL if the attribute
// does not exist or if an error occurred. Error information is stored in the
// mandatory `err` parameter.
const char *xtree_node_attr_value_get1 (struct xtree_errobj_t *err,
                                        xtree_node_t *node, const char *name);



#ifdef __cplusplus
};
#endif


#endif

