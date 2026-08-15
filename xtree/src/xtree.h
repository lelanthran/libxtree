
#ifndef H_XTREE
#define H_XTREE


#define XTREE_ERROR(...)    do {\
  fprintf (stderr, "%s:%i:", __FILE__, __LINE__);\
} while (0)

/* ***************************************************************************
 * An xtree is a generic tree suitable for storing XML, HTML or similar trees.
 * It can also store JSON trees, s-expressions, etc.
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
  xtree_node_type_ATOM = 1,
  xtree_node_type_LIST,
};

#ifdef __cplusplus
extern "C" {
#endif

// Create a new node using the specified node as a parent. If no parent is
// specified then the returned node is a root node.
xtree_node_t *xtree_node_new (xtree_node_t *parent, enum xtree_node_type_t type);

// Free the specified node, removing it from any parent if necessary.
void xtree_node_free (xtree_node_t **node);

// Dump the node recursively. Used only during testing.
void xtree_node_dump (const xtree_node_t *node, FILE *outf, size_t depth);

// Remove the i'th child from the node specified
void xtree_node_child_remove (xtree_node_t *node, size_t i);

// Sets the value of a node of type _ATOM
const char *xtree_node_value_set (xtree_node_t *node, const char *value);

// Get the value of the node of type_ATOM. If node is not of type_ATOM, NULL
// is returned
const char *xtree_node_value_get (xtree_node_t *node);

// Add an attribute to a node. In the event that the attribute with the
// specified name already exists, a new attribute with the same name is
// created and stored.
const char *xtree_node_attr_new (xtree_node_t *node, const char *name, const char *value);

// Set the first attribute with the specified name to the specified value, or
// return NULL if the attribute does not exist.
const char *xtree_node_attr_value_set (xtree_node_t *node, const char *name, const char *value);

// Return the first attribute of the specified name, or NULL if the attribute
// does not exist
const char *xtree_node_attr_value_get1 (xtree_node_t *node, const char *name);



#ifdef __cplusplus
};
#endif


#endif

