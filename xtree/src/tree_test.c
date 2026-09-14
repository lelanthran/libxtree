#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Small test program to test the xtree library */
#include "xtree_err.h"
#include "xtree.h"


#define PERROR(...)   do { \
  fprintf (stderr, "%s:%i ", __FILE__, __LINE__); \
  fprintf (stderr, __VA_ARGS__); \
} while (0)

static void dumperr (struct xtree_errobj_t *err)
{
  if (err->libcode || err->syscode) {
    xtree_errobj_dump (err, stdout);
  }
}

static int basic_test (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR ("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *rootnode = xtree_node_new (&err, NULL, "rootnode", xtree_node_type_LIST);
  xtree_node_t *tmpnode = NULL;

  if (!rootnode) {
    PERROR ("Failed to create root node\n");
    goto cleanup;
  }

  if (!(xtree_node_attr_new (&err, rootnode, "n1", "v1"))) {
    PERROR ("Failed to add first attrs to rootnode\n");
    goto cleanup;
  }

  if (!(xtree_node_attr_value_set (&err, rootnode, "n1", "v-two"))) {
    PERROR ("Failed to reset the name of the attribute\n");
    goto cleanup;
  }

  const char *tmp = NULL;
  if (!(tmp = xtree_node_attr_value_get1 (&err, rootnode, "n1"))) {
    PERROR ("Failed to retrieve the node attribute value\n");
    goto cleanup;
  }
  if ((strcmp (tmp, "v-two")) != 0) {
    PERROR ("[%s]: Expected attribute to be [%s]\n", tmp, "v-two");
    goto cleanup;
  }

  for (size_t i=0; i<4; i++) {
    char nname[20];
    char nvalue[20];
    snprintf (nname, sizeof nname, "name-%zu", i);
    snprintf (nvalue, sizeof nvalue, "value-%zu", i);
    enum xtree_node_type_t type =
      (!(i % 2))
        ? xtree_node_type_ATOM
        : xtree_node_type_LIST;

    if (!(tmpnode = xtree_node_new (&err, rootnode, nname, type))) {
      PERROR ("Failed to create childnode %zu\n", i);
      goto cleanup;
    }
    const char *set = NULL;
    switch (type) {
      case xtree_node_type_ATOM:
        set = xtree_node_value_set (&err, tmpnode, nvalue);
        break;
      case xtree_node_type_LIST:
        set = xtree_node_attr_new (&err, tmpnode, "_tag",  nname);
        break;
      case xtree_node_type_UNKNOWN:
      default:
        PERROR ("[%i]: Node type is not recognised\n", type);
        set = NULL;
        break;
    }
    if (!set) {
      PERROR ("Failed to set value or attribute");
      goto cleanup;
    }
  }

  ret = EXIT_SUCCESS;
cleanup:
  xtree_node_free (&tmpnode);
  xtree_node_free (&rootnode);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_create_root_list_node
int test_create_root_list_node (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *node = xtree_node_new (&err, NULL, "root", xtree_node_type_LIST);
  if (!node) {
    PERROR("Failed to create root LIST node\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&node);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_create_root_atom_node
int test_create_root_atom_node (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *node = xtree_node_new (&err, NULL, "atom_root", xtree_node_type_ATOM);
  if (!node) {
    PERROR("Failed to create root ATOM node\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&node);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_node_free_null_safety
int test_node_free_null_safety (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *node = xtree_node_new (&err, NULL, "test", xtree_node_type_LIST);
  if (!node) {
    PERROR("Failed to create node\n");
    goto cleanup;
  }

  // Test standard free
  xtree_node_free (&node);

  // Test freeing NULL pointer (should not crash)
  xtree_node_free (&node);
  xtree_node_free (NULL);

  ret = EXIT_SUCCESS;

cleanup:
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_atom_value_set_get
int test_atom_value_set_get (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *node = xtree_node_new (&err, NULL, "atom", xtree_node_type_ATOM);
  if (!node) {
    PERROR("Failed to create node\n");
    goto cleanup;
  }

  const char *val_in = "Hello World";
  const char *set_res = xtree_node_value_set (&err, node, val_in);
  if (!set_res) {
    PERROR("Failed to set value\n");
    goto cleanup;
  }

  if (strcmp (set_res, val_in) != 0) {
    PERROR("Set return value mismatch\n");
    goto cleanup;
  }

  const char *val_out = xtree_node_value_get (&err, node);
  if (!val_out || strcmp (val_out, val_in) != 0) {
    PERROR("Get value mismatch or NULL\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&node);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_atom_value_on_list
int test_atom_value_on_list (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *node = xtree_node_new (&err, NULL, "list", xtree_node_type_LIST);
  if (!node) {
    PERROR("Failed to create node\n");
    goto cleanup;
  }

  const char *res = xtree_node_value_set (&err, node, "Should not work?");
  if (res != NULL) {
    PERROR("Warning: Value set on LIST node returned success.\n");
    goto cleanup;
  }

  xtree_errobj_reset (&err, 0);
  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&node);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_attribute_add_get
int test_attribute_add_get (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *node = xtree_node_new (&err, NULL, "node", xtree_node_type_LIST);
  if (!node) {
    PERROR("Failed to create node\n");
    goto cleanup;
  }

  const char *key = "lang";
  const char *val = "C";

  if (!xtree_node_attr_new (&err, node, key, val)) {
    PERROR("Failed to add attribute\n");
    goto cleanup;
  }

  const char *retrieved = xtree_node_attr_value_get1 (&err, node, key);
  if (!retrieved || strcmp (retrieved, val) != 0) {
    PERROR("Failed to retrieve attribute or mismatch\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&node);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_attribute_update
int test_attribute_update (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *node = xtree_node_new (&err, NULL, "node", xtree_node_type_LIST);
  if (!node) {
    PERROR("Failed to create node\n");
    goto cleanup;
  }

  const char *key = "id";
  xtree_node_attr_new (&err, node, key, "100");

  const char *new_val = "200";
  const char *res = xtree_node_attr_value_set (&err, node, key, new_val);

  if (!res || strcmp (res, new_val) != 0) {
    PERROR("Failed to update attribute\n");
    goto cleanup;
  }

  const char *check = xtree_node_attr_value_get1 (&err, node, key);
  if (!check) {
    PERROR ("Attribute retrieval returned NULL\n");
    goto cleanup;
  }
  if (strcmp (check, new_val) != 0) {
    PERROR("Attribute value not actually updated\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&node);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_attribute_update_nonexistent
int test_attribute_update_nonexistent (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *node = xtree_node_new (&err, NULL, "node", xtree_node_type_LIST);
  if (!node) {
    PERROR("Failed to create node\n");
    goto cleanup;
  }

  const char *res = xtree_node_attr_value_set (&err, node, "nonexistent", "val");
  if (res != NULL) {
    PERROR("Setting nonexistent attribute should return NULL per spec\n");
    goto cleanup;
  }

  xtree_errobj_reset (&err, 0);
  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&node);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_attribute_duplicate_names
int test_attribute_duplicate_names (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *node = xtree_node_new (&err, NULL, "node", xtree_node_type_LIST);
  if (!node) {
    PERROR("Failed to create node\n");
    goto cleanup;
  }

  const char *key = "color";
  xtree_node_attr_new (&err, node, key, "red");
  xtree_node_attr_new (&err, node, key, "blue");

  const char *res = xtree_node_attr_value_get1 (&err, node, key);
  if (!res || strcmp (res, "red") != 0) {
    PERROR("get1 did not return the first attribute\n");
    goto cleanup;
  }

  xtree_node_attr_value_set (&err, node, key, "green");
  res = xtree_node_attr_value_get1 (&err, node, key);
  if (!res || strcmp (res, "green") != 0) {
    PERROR("Updating did not update the first attribute\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&node);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_child_append
int test_child_append (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *root = xtree_node_new (&err, NULL, "root", xtree_node_type_LIST);
  if (!root) goto cleanup;

  xtree_node_t *children[5];
  for (int i = 0; i < 5; i++) {
    children[i] = xtree_node_new (&err, root, "child", xtree_node_type_ATOM);
    if (!children[i]) {
      PERROR("Failed to create child %d\n", i);
      goto cleanup;
    }
  }

  size_t count = xtree_node_child_count (root);
  if (count != 5) {
    PERROR("Expected 5 children, got %zu\n", count);
    goto cleanup;
  }

  for (size_t i = 0; i < 5; i++) {
    if (xtree_node_child_get (&err, root, i) != children[i]) {
      PERROR("Child at position %zu does not match appended child\n", i);
      goto cleanup;
    }
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&root);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_child_detach_middle
int test_child_detach_middle (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *root = xtree_node_new (&err, NULL, "root", xtree_node_type_LIST);
  if (!root) goto cleanup;

  xtree_node_t *c1 = xtree_node_new (&err, root, "c1", xtree_node_type_ATOM);
  xtree_node_t *c2 = xtree_node_new (&err, root, "c2", xtree_node_type_ATOM);
  xtree_node_t *c3 = xtree_node_new (&err, root, "c3", xtree_node_type_ATOM);

  if (!c1 || !c2 || !c3) {
    PERROR("Failed to create children\n");
    goto cleanup;
  }

  (void)c1; // Used for setup only
  (void)c3; // Used for setup only


  xtree_node_t *detached = xtree_node_child_detach (&err, root, 1);
  if (!detached) {
    PERROR("Failed to detach child\n");
    goto cleanup;
  }

  if (detached != c2) {
    PERROR("Detached node pointer mismatch\n");
    goto cleanup;
  }

  xtree_node_free (&detached);
  xtree_errobj_reset (&err, 0);
  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&root);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_child_detach_bounds
int test_child_detach_bounds (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *root = xtree_node_new (&err, NULL, "root", xtree_node_type_LIST);
  if (!root) goto cleanup;

  if (!xtree_node_new (&err, root, "c1", xtree_node_type_ATOM)) {
      PERROR("Failed to create child\n");
      goto cleanup;
  }

  xtree_node_t *detached = xtree_node_child_detach (&err, root, 10);
  if (detached != NULL) {
    PERROR("Detaching out of bounds should return NULL\n");
    goto cleanup;
  }

  xtree_errobj_reset (&err, 0);
  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&root);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_child_insert_at_position
int test_child_insert_at_position (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *root = xtree_node_new (&err, NULL, "root", xtree_node_type_LIST);
  if (!root) goto cleanup;

  xtree_node_t *c1 = xtree_node_new (&err, root, "c1", xtree_node_type_ATOM);
  xtree_node_t *c3 = xtree_node_new (&err, root, "c3", xtree_node_type_ATOM);

  if (!c1 || !c3) goto cleanup;

  xtree_node_t *c2 = xtree_node_new (&err, NULL, "c2", xtree_node_type_ATOM);
  if (!c2) goto cleanup;

  xtree_node_t *res = xtree_node_child_attach (&err, root, c2, 1);
  if (!res) {
    PERROR("Failed to attach child at position\n");
    goto cleanup;
  }

  size_t count = xtree_node_child_count (root);
  if (count != 3) {
    PERROR("Expected 3 children, got %zu\n", count);
    goto cleanup;
  }

  if (xtree_node_child_get (&err, root, 0) != c1) {
    PERROR("Position 0 should be c1\n");
    goto cleanup;
  }
  if (xtree_node_child_get (&err, root, 1) != c2) {
    PERROR("Position 1 should be c2 (inserted)\n");
    goto cleanup;
  }
  if (xtree_node_child_get (&err, root, 2) != c3) {
    PERROR("Position 2 should be c3\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&root);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_child_attach_bounds_append
int test_child_attach_bounds_append (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *root = xtree_node_new (&err, NULL, "root", xtree_node_type_LIST);
  if (!root) goto cleanup;

  xtree_node_t *c1 = xtree_node_new (&err, root, "c1", xtree_node_type_ATOM);
  if (!c1) {
    PERROR("Failed to create c1\n");
    goto cleanup;
  }

  xtree_node_t *c_new = xtree_node_new (&err, NULL, "c_new", xtree_node_type_ATOM);
  if (!c_new) goto cleanup;

  xtree_node_t *res = xtree_node_child_attach (&err, root, c_new, 100);
  if (!res) {
    PERROR("Failed to attach child at out of bounds position\n");
    goto cleanup;
  }

  size_t count = xtree_node_child_count (root);
  if (count != 2) {
    PERROR("Expected 2 children, got %zu\n", count);
    goto cleanup;
  }

  if (xtree_node_child_get (&err, root, 0) != c1) {
    PERROR("Position 0 should be c1\n");
    goto cleanup;
  }
  if (xtree_node_child_get (&err, root, 1) != c_new) {
    PERROR("Position 1 should be c_new (appended)\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&root);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_node_reparenting
int test_node_reparenting (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *parent_a = xtree_node_new (&err, NULL, "parent_a", xtree_node_type_LIST);
  xtree_node_t *parent_b = xtree_node_new (&err, NULL, "parent_b", xtree_node_type_LIST);

  if (!parent_a || !parent_b) goto cleanup;

  xtree_node_t *child = xtree_node_new (&err, parent_a, "child", xtree_node_type_ATOM);
  if (!child) goto cleanup;

  if (xtree_node_child_count (parent_a) != 1) {
    PERROR("parent_a should start with 1 child\n");
    goto cleanup;
  }
  if (xtree_node_child_count (parent_b) != 0) {
    PERROR("parent_b should start with 0 children\n");
    goto cleanup;
  }

  xtree_node_t *res = xtree_node_child_attach (&err, parent_b, child, 0);
  if (!res) {
    PERROR("Failed to reparent node\n");
    goto cleanup;
  }

  if (xtree_node_child_count (parent_a) != 0) {
    PERROR("parent_a should have 0 children after reparenting, got %zu\n",
           xtree_node_child_count (parent_a));
    goto cleanup;
  }
  if (xtree_node_child_count (parent_b) != 1) {
    PERROR("parent_b should have 1 child after reparenting\n");
    goto cleanup;
  }
  if (xtree_node_child_get (&err, parent_b, 0) != child) {
    PERROR("parent_b's child should be the reparented node\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&parent_a);
  xtree_node_free (&parent_b);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_node_type_string
int test_node_type_string (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  const char *s_unknown = xtree_node_type_string (xtree_node_type_UNKNOWN);
  const char *s_atom    = xtree_node_type_string (xtree_node_type_ATOM);
  const char *s_list    = xtree_node_type_string (xtree_node_type_LIST);

  if (!s_unknown || !s_atom || !s_list) {
    PERROR("xtree_node_type_string returned NULL for a known type\n");
    goto cleanup;
  }

  if (strcmp (s_unknown, "xtree_node_type_UNKNOWN") != 0) {
    PERROR("Expected UNKNOWN, got [%s]\n", s_unknown);
    goto cleanup;
  }
  if (strcmp (s_atom, "xtree_node_type_ATOM") != 0) {
    PERROR("Expected ATOM, got [%s]\n", s_atom);
    goto cleanup;
  }
  if (strcmp (s_list, "xtree_node_type_LIST") != 0) {
    PERROR("Expected LIST, got [%s]\n", s_list);
    goto cleanup;
  }

  // Out of range value should not crash
  const char *s_bogus = xtree_node_type_string ((enum xtree_node_type_t)999);
  if (!s_bogus) {
    PERROR("xtree_node_type_string returned NULL for out-of-range value\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_node_name_get_set
int test_node_name_get_set (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *node = xtree_node_new (&err, NULL, "initial", xtree_node_type_LIST);
  if (!node) {
    PERROR("Failed to create node\n");
    goto cleanup;
  }

  const char *name = xtree_node_name_get (node);
  if (!name || strcmp (name, "initial") != 0) {
    PERROR("Expected initial name, got [%s]\n", name ? name : "(null)");
    goto cleanup;
  }

  const char *set_res = xtree_node_name_set (&err, node, "changed");
  if (!set_res || strcmp (set_res, "changed") != 0) {
    PERROR("Failed to set node name\n");
    goto cleanup;
  }

  name = xtree_node_name_get (node);
  if (!name || strcmp (name, "changed") != 0) {
    PERROR("Name was not actually updated, got [%s]\n", name ? name : "(null)");
    goto cleanup;
  }

  // Setting NULL should default to ""
  set_res = xtree_node_name_set (&err, node, NULL);
  if (!set_res) {
    PERROR("Setting name to NULL failed\n");
    goto cleanup;
  }
  name = xtree_node_name_get (node);
  if (!name || strcmp (name, "") != 0) {
    PERROR("Expected empty string after NULL name set, got [%s]\n",
           name ? name : "(null)");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&node);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_node_name_optional
int test_node_name_optional (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *node = xtree_node_new (&err, NULL, NULL, xtree_node_type_LIST);
  if (!node) {
    PERROR("Failed to create node with NULL name\n");
    goto cleanup;
  }

  const char *name = xtree_node_name_get (node);
  if (!name) {
    PERROR("Name should default to non-NULL (\"\")\n");
    goto cleanup;
  }
  if (strcmp (name, "") != 0) {
    PERROR("Expected default name to be \"\", got [%s]\n", name);
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&node);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_node_type_get
int test_node_type_get (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *atom = xtree_node_new (&err, NULL, "a", xtree_node_type_ATOM);
  xtree_node_t *list = xtree_node_new (&err, NULL, "l", xtree_node_type_LIST);

  if (!atom || !list) {
    PERROR("Failed to create nodes\n");
    goto cleanup;
  }

  if (xtree_node_type_get (atom) != xtree_node_type_ATOM) {
    PERROR("Expected ATOM type\n");
    goto cleanup;
  }
  if (xtree_node_type_get (list) != xtree_node_type_LIST) {
    PERROR("Expected LIST type\n");
    goto cleanup;
  }

  // NULL should return UNKNOWN
  if (xtree_node_type_get (NULL) != xtree_node_type_UNKNOWN) {
    PERROR("Expected UNKNOWN for NULL node\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&atom);
  xtree_node_free (&list);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_node_type_get_invalid_create
int test_node_type_get_invalid_create (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  // Only ATOM and LIST are allowable per the header docs
  xtree_node_t *bad = xtree_node_new (&err, NULL, "bad", xtree_node_type_UNKNOWN);
  if (bad != NULL) {
    PERROR("Creating node with UNKNOWN type should return NULL\n");
    xtree_node_free (&bad);
    goto cleanup;
  }

  xtree_errobj_reset (&err, 0);
  ret = EXIT_SUCCESS;

cleanup:
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_node_parent
int test_node_parent (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *root = xtree_node_new (&err, NULL, "root", xtree_node_type_LIST);
  if (!root) goto cleanup;

  // A root node should have no parent
  if (xtree_node_parent (&err, root) != NULL) {
    PERROR("Root node should have NULL parent\n");
    goto cleanup;
  }

  xtree_node_t *child = xtree_node_new (&err, root, "child", xtree_node_type_ATOM);
  if (!child) {
    PERROR("Failed to create child\n");
    goto cleanup;
  }

  if (xtree_node_parent (&err, child) != root) {
    PERROR("Child's parent should be root\n");
    goto cleanup;
  }

  // After detaching, parent should become NULL
  xtree_node_t *detached = xtree_node_child_detach (&err, root, 0);
  if (!detached) {
    PERROR("Failed to detach child\n");
    goto cleanup;
  }

  if (xtree_node_parent (&err, detached) != NULL) {
    PERROR("Detached node should have NULL parent\n");
    xtree_node_free (&detached);
    goto cleanup;
  }

  xtree_node_free (&detached);
  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&root);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_value_append
int test_value_append (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *node = xtree_node_new (&err, NULL, "atom", xtree_node_type_ATOM);
  if (!node) {
    PERROR("Failed to create node\n");
    goto cleanup;
  }

  if (!xtree_node_value_set (&err, node, "Hello")) {
    PERROR("Failed to set initial value\n");
    goto cleanup;
  }

  const char *appended = xtree_node_value_append (&err, node, " World");
  if (!appended) {
    PERROR("Failed to append to value\n");
    goto cleanup;
  }
  if (strcmp (appended, "Hello World") != 0) {
    PERROR("Expected [Hello World], got [%s]\n", appended);
    goto cleanup;
  }

  const char *check = xtree_node_value_get (&err, node);
  if (!check || strcmp (check, "Hello World") != 0) {
    PERROR("Value get after append mismatch, got [%s]\n",
           check ? check : "(null)");
    goto cleanup;
  }

  // Append to a LIST node must fail
  xtree_node_t *list = xtree_node_new (&err, NULL, "list", xtree_node_type_LIST);
  if (!list) {
    PERROR("Failed to create list node\n");
    goto cleanup;
  }
  xtree_errobj_clrerr (&err);
  const char *bad = xtree_node_value_append (&err, list, "nope");
  if (bad != NULL) {
    PERROR("Append on LIST node should return NULL\n");
    xtree_node_free (&list);
    goto cleanup;
  }
  xtree_node_free (&list);

  xtree_errobj_reset (&err, 0);
  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&node);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_child_find
int test_child_find (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *root = xtree_node_new (&err, NULL, "root", xtree_node_type_LIST);
  if (!root) goto cleanup;

  xtree_node_t *c0 = xtree_node_new (&err, root, "c0", xtree_node_type_ATOM);
  xtree_node_t *c1 = xtree_node_new (&err, root, "c1", xtree_node_type_ATOM);
  xtree_node_t *c2 = xtree_node_new (&err, root, "c2", xtree_node_type_ATOM);

  if (!c0 || !c1 || !c2) {
    PERROR("Failed to create children\n");
    goto cleanup;
  }

  if (xtree_node_child_find (&err, root, c0) != 0) {
    PERROR("c0 should be at position 0\n");
    goto cleanup;
  }
  if (xtree_node_child_find (&err, root, c1) != 1) {
    PERROR("c1 should be at position 1\n");
    goto cleanup;
  }
  if (xtree_node_child_find (&err, root, c2) != 2) {
    PERROR("c2 should be at position 2\n");
    goto cleanup;
  }

  // A node not in the tree should return (size_t)-1
  xtree_node_t *stranger = xtree_node_new (&err, NULL, "stranger", xtree_node_type_ATOM);
  if (!stranger) {
    PERROR("Failed to create stranger node\n");
    goto cleanup;
  }
  size_t pos = xtree_node_child_find (&err, root, stranger);
  if (pos != (size_t)-1) {
    PERROR("Stranger should not be found, got %zu\n", pos);
    xtree_node_free (&stranger);
    goto cleanup;
  }
  xtree_node_free (&stranger);

  xtree_errobj_reset (&err, 0);
  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&root);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_child_append_api
int test_child_append_api (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *root = xtree_node_new (&err, NULL, "root", xtree_node_type_LIST);
  if (!root) goto cleanup;

  xtree_node_t *a = xtree_node_new (&err, NULL, "a", xtree_node_type_ATOM);
  xtree_node_t *b = xtree_node_new (&err, NULL, "b", xtree_node_type_ATOM);
  if (!a || !b) {
    PERROR("Failed to create standalone children\n");
    goto cleanup;
  }

  xtree_node_t *res = xtree_node_child_append (&err, root, a);
  if (res != a) {
    PERROR("child_append should return the appended child\n");
    goto cleanup;
  }
  res = xtree_node_child_append (&err, root, b);
  if (res != b) {
    PERROR("child_append should return the appended child\n");
    goto cleanup;
  }

  if (xtree_node_child_count (root) != 2) {
    PERROR("Expected 2 children after append, got %zu\n",
           xtree_node_child_count (root));
    goto cleanup;
  }
  if (xtree_node_child_get (&err, root, 0) != a) {
    PERROR("First appended child mismatch\n");
    goto cleanup;
  }
  if (xtree_node_child_get (&err, root, 1) != b) {
    PERROR("Second appended child mismatch\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&root);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_child_append_invalid
int test_child_append_invalid (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *root = xtree_node_new (&err, NULL, "root", xtree_node_type_LIST);
  if (!root) goto cleanup;

  // Appending NULL should fail
  xtree_errobj_clrerr (&err);
  if (xtree_node_child_append (&err, root, NULL) != NULL) {
    PERROR("Appending NULL child should return NULL\n");
    goto cleanup;
  }

  // Appending to a NULL parent should fail
  xtree_node_t *orphan = xtree_node_new (&err, NULL, "orphan", xtree_node_type_ATOM);
  if (!orphan) {
    PERROR("Failed to create orphan\n");
    goto cleanup;
  }
  xtree_errobj_clrerr (&err);
  if (xtree_node_child_append (&err, NULL, orphan) != NULL) {
    PERROR("Appending to NULL parent should return NULL\n");
    xtree_node_free (&orphan);
    goto cleanup;
  }
  xtree_node_free (&orphan);

  // Appending to an ATOM parent should fail
  xtree_node_t *atom_parent = xtree_node_new (&err, NULL, "ap", xtree_node_type_ATOM);
  xtree_node_t *child = xtree_node_new (&err, NULL, "c", xtree_node_type_ATOM);
  if (!atom_parent || !child) {
    PERROR("Failed to create atom parent / child\n");
    xtree_node_free (&atom_parent);
    xtree_node_free (&child);
    goto cleanup;
  }
  xtree_errobj_clrerr (&err);
  if (xtree_node_child_append (&err, atom_parent, child) != NULL) {
    PERROR("Appending to ATOM parent should return NULL\n");
    xtree_node_free (&atom_parent);
    xtree_node_free (&child);
    goto cleanup;
  }
  xtree_node_free (&atom_parent);
  xtree_node_free (&child);

  xtree_errobj_reset (&err, 0);
  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&root);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_attr_count
int test_attr_count (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *node = xtree_node_new (&err, NULL, "node", xtree_node_type_LIST);
  if (!node) {
    PERROR("Failed to create node\n");
    goto cleanup;
  }

  if (xtree_node_attr_count (node) != 0) {
    PERROR("Fresh node should have 0 attributes\n");
    goto cleanup;
  }

  xtree_node_attr_new (&err, node, "a", "1");
  xtree_node_attr_new (&err, node, "b", "2");
  xtree_node_attr_new (&err, node, "a", "3"); // duplicate name

  if (xtree_node_attr_count (node) != 3) {
    PERROR("Expected 3 attributes, got %zu\n", xtree_node_attr_count (node));
    goto cleanup;
  }

  // NULL node should return 0
  if (xtree_node_attr_count (NULL) != 0) {
    PERROR("NULL node should return 0 attributes\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&node);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_attr_value_get_named_ordinal
int test_attr_value_get_named_ordinal (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *node = xtree_node_new (&err, NULL, "node", xtree_node_type_LIST);
  if (!node) {
    PERROR("Failed to create node\n");
    goto cleanup;
  }

  xtree_node_attr_new (&err, node, "color", "red");
  xtree_node_attr_new (&err, node, "size",  "large");
  xtree_node_attr_new (&err, node, "color", "blue"); // duplicate name

  // The ordinal counts only attributes of the specified name: the 0th
  // "color" is "red", the 1st is "blue", and the duplicate "color"s do
  // not shift the 0th "size".
  const char *c0 = xtree_node_attr_value_get (&err, node, "color", 0);
  const char *c1 = xtree_node_attr_value_get (&err, node, "color", 1);
  const char *s0 = xtree_node_attr_value_get (&err, node, "size", 0);

  if (!c0 || strcmp (c0, "red") != 0) {
    PERROR("0th [color] expected [red], got [%s]\n", c0 ? c0 : "(null)");
    goto cleanup;
  }
  if (!c1 || strcmp (c1, "blue") != 0) {
    PERROR("1st [color] expected [blue], got [%s]\n", c1 ? c1 : "(null)");
    goto cleanup;
  }
  if (!s0 || strcmp (s0, "large") != 0) {
    PERROR("0th [size] expected [large], got [%s]\n", s0 ? s0 : "(null)");
    goto cleanup;
  }

  // Requesting the 2nd "color" when only two exist is out of bounds and
  // must return NULL with the error object set.
  xtree_errobj_reset (&err, 1024);
  if (xtree_node_attr_value_get (&err, node, "color", 2) != NULL) {
    PERROR("Out-of-range ordinal should return NULL\n");
    goto cleanup;
  }
  if (err.libcode == xtree_errcode_NONE) {
    PERROR("Out-of-range ordinal should set the error object\n");
    goto cleanup;
  }

  // An unknown name must also fail.
  xtree_errobj_reset (&err, 1024);
  if (xtree_node_attr_value_get (&err, node, "nonexistent", 0) != NULL) {
    PERROR("Unknown attribute name should return NULL\n");
    goto cleanup;
  }

  // Neither node nor name may be NULL.
  xtree_errobj_reset (&err, 1024);
  if (xtree_node_attr_value_get (&err, NULL, "color", 0) != NULL) {
    PERROR("NULL node should return NULL\n");
    goto cleanup;
  }
  xtree_errobj_reset (&err, 1024);
  if (xtree_node_attr_value_get (&err, node, NULL, 0) != NULL) {
    PERROR("NULL name should return NULL\n");
    goto cleanup;
  }

  xtree_errobj_reset (&err, 0);
  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&node);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_attr_duplicate_names_get1_vs_ordinal
int test_attr_duplicate_names_get1_vs_ordinal (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *node = xtree_node_new (&err, NULL, "node", xtree_node_type_LIST);
  if (!node) {
    PERROR("Failed to create node\n");
    goto cleanup;
  }

  xtree_node_attr_new (&err, node, "k", "v1");
  xtree_node_attr_new (&err, node, "k", "v2");
  xtree_node_attr_new (&err, node, "k", "v3");

  // get1 returns the first match
  const char *first = xtree_node_attr_value_get1 (&err, node, "k");
  if (!first || strcmp (first, "v1") != 0) {
    PERROR("get1 expected [v1], got [%s]\n", first ? first : "(null)");
    goto cleanup;
  }

  // The ordinal get should expose all three in insertion order
  const char *i0 = xtree_node_attr_value_get (&err, node, "k", 0);
  const char *i1 = xtree_node_attr_value_get (&err, node, "k", 1);
  const char *i2 = xtree_node_attr_value_get (&err, node, "k", 2);
  if (!i0 || strcmp (i0, "v1") != 0 ||
      !i1 || strcmp (i1, "v2") != 0 ||
      !i2 || strcmp (i2, "v3") != 0) {
    PERROR("Ordinal duplicate attribute mismatch\n");
    goto cleanup;
  }

  // attr_count should reflect all three
  if (xtree_node_attr_count (node) != 3) {
    PERROR("Expected 3 attributes, got %zu\n", xtree_node_attr_count (node));
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&node);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_parent_after_attach
int test_parent_after_attach (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *parent_a = xtree_node_new (&err, NULL, "a", xtree_node_type_LIST);
  xtree_node_t *parent_b = xtree_node_new (&err, NULL, "b", xtree_node_type_LIST);
  if (!parent_a || !parent_b) goto cleanup;

  xtree_node_t *child = xtree_node_new (&err, parent_a, "c", xtree_node_type_ATOM);
  if (!child) goto cleanup;

  if (xtree_node_parent (&err, child) != parent_a) {
    PERROR("Child should report parent_a\n");
    goto cleanup;
  }

  if (xtree_node_child_attach (&err, parent_b, child, 0) != child) {
    PERROR("Failed to reparent child\n");
    goto cleanup;
  }

  if (xtree_node_parent (&err, child) != parent_b) {
    PERROR("Child should report parent_b after reparenting\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&parent_a);
  xtree_node_free (&parent_b);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_attr_new_null_params
int test_attr_new_null_params (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *node = xtree_node_new (&err, NULL, "node", xtree_node_type_LIST);
  if (!node) {
    PERROR("Failed to create node\n");
    goto cleanup;
  }

  xtree_errobj_reset (&err, 1024);
  if (xtree_node_attr_new (&err, NULL, "k", "v") != NULL) {
    PERROR("attr_new with NULL node should return NULL\n");
    goto cleanup;
  }
  if (err.libcode == xtree_errcode_NONE) {
    PERROR("attr_new with NULL node should set the error object\n");
    goto cleanup;
  }

  xtree_errobj_reset (&err, 1024);
  if (xtree_node_attr_new (&err, node, NULL, "v") != NULL) {
    PERROR("attr_new with NULL name should return NULL\n");
    goto cleanup;
  }
  if (err.libcode == xtree_errcode_NONE) {
    PERROR("attr_new with NULL name should set the error object\n");
    goto cleanup;
  }

  xtree_errobj_reset (&err, 1024);
  if (xtree_node_attr_new (&err, node, "k", NULL) != NULL) {
    PERROR("attr_new with NULL value should return NULL\n");
    goto cleanup;
  }
  if (err.libcode == xtree_errcode_NONE) {
    PERROR("attr_new with NULL value should set the error object\n");
    goto cleanup;
  }

  // Failed calls must not have added anything.
  if (xtree_node_attr_count (node) != 0) {
    PERROR("Failed attr_new calls must not add attributes\n");
    goto cleanup;
  }

  xtree_errobj_reset (&err, 0);
  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&node);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_node_new_invalid_parent_and_type
int test_node_new_invalid_parent_and_type (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  // The parent must be of type _LIST.
  xtree_node_t *atom = xtree_node_new (&err, NULL, "atom", xtree_node_type_ATOM);
  if (!atom) {
    PERROR("Failed to create atom node\n");
    goto cleanup;
  }

  xtree_errobj_reset (&err, 1024);
  xtree_node_t *child = xtree_node_new (&err, atom, "child", xtree_node_type_ATOM);
  if (child != NULL) {
    PERROR("Creating a child under an ATOM parent should return NULL\n");
    xtree_node_free (&child);
    goto cleanup;
  }
  if (err.libcode == xtree_errcode_NONE) {
    PERROR("Creating a child under an ATOM parent should set the error object\n");
    goto cleanup;
  }

  // Only _ATOM and _LIST are allowable; out-of-range types must error too.
  xtree_errobj_reset (&err, 1024);
  xtree_node_t *bad = xtree_node_new (&err, NULL, "bad",
                                      (enum xtree_node_type_t)999);
  if (bad != NULL) {
    PERROR("Creating a node with an out-of-range type should return NULL\n");
    xtree_node_free (&bad);
    goto cleanup;
  }
  if (err.libcode == xtree_errcode_NONE) {
    PERROR("Out-of-range node type should set the error object\n");
    goto cleanup;
  }

  xtree_errobj_reset (&err, 1024);
  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&atom);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_value_append_no_existing
int test_value_append_no_existing (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *node = xtree_node_new (&err, NULL, "atom", xtree_node_type_ATOM);
  if (!node) {
    PERROR("Failed to create node\n");
    goto cleanup;
  }

  // With no existing value, the appended string becomes the value.
  const char *val = xtree_node_value_append (&err, node, "first");
  if (!val || strcmp (val, "first") != 0) {
    PERROR("Expected [first], got [%s]\n", val ? val : "(null)");
    goto cleanup;
  }
  const char *check = xtree_node_value_get (&err, node);
  if (!check || strcmp (check, "first") != 0) {
    PERROR("Value get after append mismatch, got [%s]\n",
           check ? check : "(null)");
    goto cleanup;
  }

  // And appending again concatenates onto it.
  val = xtree_node_value_append (&err, node, " second");
  if (!val || strcmp (val, "first second") != 0) {
    PERROR("Expected [first second], got [%s]\n", val ? val : "(null)");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&node);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_free_detaches_from_parent
int test_free_detaches_from_parent (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *root = xtree_node_new (&err, NULL, "root", xtree_node_type_LIST);
  if (!root) goto cleanup;

  xtree_node_t *child = xtree_node_new (&err, root, "child", xtree_node_type_ATOM);
  if (!child) {
    PERROR("Failed to create child\n");
    goto cleanup;
  }
  if (xtree_node_child_count (root) != 1) {
    PERROR("Expected 1 child before free\n");
    goto cleanup;
  }

  // Freeing an attached node must remove it from its parent.
  xtree_node_free (&child);
  if (xtree_node_child_count (root) != 0) {
    PERROR("Freed child was not detached from parent\n");
    goto cleanup;
  }

  // The header now documents that children are recursively freed, so
  // freeing a root with a subtree must be safe and complete.
  xtree_node_t *mid = xtree_node_new (&err, root, "mid", xtree_node_type_LIST);
  if (!mid) {
    PERROR("Failed to create mid node\n");
    goto cleanup;
  }
  if (!xtree_node_new (&err, mid, "leaf", xtree_node_type_ATOM)) {
    PERROR("Failed to create leaf node\n");
    goto cleanup;
  }
  xtree_node_free (&root); // recursively frees mid and leaf
  if (root != NULL) {
    PERROR("xtree_node_free should set the pointer to NULL\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&root); // NULL-safe, may already be NULL
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_child_count_and_get_invalid
int test_child_count_and_get_invalid (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *atom = xtree_node_new (&err, NULL, "atom", xtree_node_type_ATOM);
  xtree_node_t *root = xtree_node_new (&err, NULL, "root", xtree_node_type_LIST);
  if (!atom || !root) {
    PERROR("Failed to create nodes\n");
    goto cleanup;
  }

  // child_count: NULL or non-LIST node yields .
  if (xtree_node_child_count (NULL) != 0) {
    PERROR("NULL node should have 0 children\n");
    goto cleanup;
  }
  if (xtree_node_child_count (atom) != 0) {
    PERROR("ATOM node should have 0 children\n");
    goto cleanup;
  }

  if (!xtree_node_new (&err, root, "c1", xtree_node_type_ATOM)) {
    PERROR("Failed to create child\n");
    goto cleanup;
  }

  // child_get out of bounds must fail and set the error object.
  xtree_errobj_reset (&err, 1024);
  if (xtree_node_child_get (&err, root, 5) != NULL) {
    PERROR("Out-of-bounds child_get should return NULL\n");
    goto cleanup;
  }
  if (err.libcode == xtree_errcode_NONE) {
    PERROR("Out-of-bounds child_get should set the error object\n");
    goto cleanup;
  }

  // A non-LIST node has no children to get.
  xtree_errobj_reset (&err, 1024);
  if (xtree_node_child_get (&err, atom, 0) != NULL) {
    PERROR("child_get on an ATOM node should return NULL\n");
    goto cleanup;
  }

  xtree_errobj_reset (&err, 0);
  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&atom);
  xtree_node_free (&root);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_parent_err_only_on_error
int test_parent_err_only_on_error (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *root = xtree_node_new (&err, NULL, "root", xtree_node_type_LIST);
  if (!root) goto cleanup;

  // A root node has a NULL parent, but that is NOT an error: per the
  // header the error object is only set on error.
  if (xtree_node_parent (&err, root) != NULL) {
    PERROR("Root node should have NULL parent\n");
    goto cleanup;
  }
  if (err.libcode != xtree_errcode_NONE) {
    PERROR("Error object must not be set for a root node\n");
    goto cleanup;
  }

  // An actual error (NULL node) must set it.
  xtree_errobj_reset (&err, 1024);
  if (xtree_node_parent (&err, NULL) != NULL) {
    PERROR("NULL node should return NULL parent\n");
    goto cleanup;
  }
  if (err.libcode == xtree_errcode_NONE) {
    PERROR("NULL node should set the error object\n");
    goto cleanup;
  }

  xtree_errobj_reset (&err, 0);
  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&root);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_node_dump
int test_node_dump (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };
  FILE *sink = NULL;

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *root = xtree_node_new (&err, NULL, "root", xtree_node_type_LIST);
  if (!root) goto cleanup;

  xtree_node_attr_new (&err, root, "version", "1.0");

  xtree_node_t *child = xtree_node_new (&err, root, "item", xtree_node_type_ATOM);
  if (!child) {
    PERROR("Failed to create child\n");
    goto cleanup;
  }
  xtree_node_value_set (&err, child, "Data");
  xtree_node_attr_new (&err, child, "id", "101");

  // Dump to a scratch file so the golden-output diff on stdout is not
  // affected; verify something was actually written.
  sink = tmpfile ();
  if (!sink) {
    PERROR("tmpfile failed\n");
    goto cleanup;
  }
  xtree_node_dump (root, sink, 0);
  if (fflush (sink) != 0) {
    PERROR("fflush of dump output failed\n");
    goto cleanup;
  }
  if (ftell (sink) <= 0) {
    PERROR("xtree_node_dump produced no output\n");
    goto cleanup;
  }
  fclose (sink);
  sink = NULL;

  ret = EXIT_SUCCESS;

cleanup:
  if (sink) fclose (sink);
  xtree_node_free (&root);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}

// name: test_child_append_reparents
int test_child_append_reparents (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = { 0 };

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR("Failed to allocate error object\n");
    goto cleanup;
  }

  xtree_node_t *parent_a = xtree_node_new (&err, NULL, "parent_a", xtree_node_type_LIST);
  xtree_node_t *parent_b = xtree_node_new (&err, NULL, "parent_b", xtree_node_type_LIST);
  if (!parent_a || !parent_b) goto cleanup;

  xtree_node_t *child = xtree_node_new (&err, parent_a, "child", xtree_node_type_ATOM);
  if (!child) goto cleanup;

  if (xtree_node_child_count (parent_a) != 1) {
    PERROR("parent_a should start with 1 child\n");
    goto cleanup;
  }
  if (xtree_node_child_count (parent_b) != 0) {
    PERROR("parent_b should start with 0 children\n");
    goto cleanup;
  }
  if (xtree_node_parent (&err, child) != parent_a) {
    PERROR("Child should initially report parent_a\n");
    goto cleanup;
  }

  // Appending a child that is already attached to another parent must
  // re-parent it: the child is removed from parent_a and appended to
  // parent_b. This mirrors the re-parenting contract documented for
  // xtree_node_child_attach().
  xtree_node_t *res = xtree_node_child_append (&err, parent_b, child);
  if (res != child) {
    PERROR("child_append should return the appended child\n");
    goto cleanup;
  }

  if (xtree_node_child_count (parent_a) != 0) {
    PERROR("parent_a should have 0 children after re-parenting, got %zu\n",
           xtree_node_child_count (parent_a));
    goto cleanup;
  }
  if (xtree_node_child_count (parent_b) != 1) {
    PERROR("parent_b should have 1 child after re-parenting, got %zu\n",
           xtree_node_child_count (parent_b));
    goto cleanup;
  }
  if (xtree_node_child_get (&err, parent_b, 0) != child) {
    PERROR("parent_b's child should be the re-parented node\n");
    goto cleanup;
  }
  if (xtree_node_parent (&err, child) != parent_b) {
    PERROR("Child should report parent_b after re-parenting\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&parent_a);
  xtree_node_free (&parent_b);
  dumperr (&err);
  xtree_errobj_reset (&err, 0);
  return ret;
}
int main (void)
{
  int errcount = 0;
#define TEST(x)     { #x, x }

  static const struct {
    const char *name;
    int (*fptr) (void);
  } tests[] = {
    TEST (basic_test),
    TEST (test_create_root_list_node),
    TEST (test_create_root_atom_node),
    TEST (test_node_free_null_safety),
    TEST (test_atom_value_set_get),
    TEST (test_atom_value_on_list),
    TEST (test_attribute_add_get),
    TEST (test_attribute_update),
    TEST (test_attribute_update_nonexistent),
    TEST (test_attribute_duplicate_names),
    TEST (test_child_append),
    TEST (test_child_detach_middle),
    TEST (test_child_detach_bounds),
    TEST (test_child_insert_at_position),
    TEST (test_child_attach_bounds_append),
    TEST (test_node_reparenting),
    /* New tests for previously uncovered API */
    TEST (test_node_type_string),
    TEST (test_node_name_get_set),
    TEST (test_node_name_optional),
    TEST (test_node_type_get),
    TEST (test_node_type_get_invalid_create),
    TEST (test_node_parent),
    TEST (test_value_append),
    TEST (test_child_find),
    TEST (test_child_append_api),
    TEST (test_child_append_invalid),
    TEST (test_attr_count),
    TEST (test_attr_value_get_named_ordinal),
    TEST (test_attr_duplicate_names_get1_vs_ordinal),
    TEST (test_parent_after_attach),
    /* Tests for the clarified contract */
    TEST (test_attr_new_null_params),
    TEST (test_node_new_invalid_parent_and_type),
    TEST (test_value_append_no_existing),
    TEST (test_free_detaches_from_parent),
    TEST (test_child_count_and_get_invalid),
    TEST (test_parent_err_only_on_error),
    TEST (test_node_dump),
    TEST (test_child_append_reparents),
  };
#undef TEST

  static const size_t ntests = sizeof tests / sizeof tests[0];

  printf ("testing tree library\n"
          "Diff this output against ./tests/outputs/tree_test.expected\n");
  for (size_t i=0; i<ntests; i++) {
    int rc = tests[i].fptr ();
    printf ("[%s]: %s\n", rc ? "failed" : "passed", tests[i].name);
    errcount += rc ? 1 : 0;
  }
  if (errcount) {
    printf ("FAILED: %i test failed\n", errcount);
  } else {
    printf ("PASSED: %zu tests passed\n", ntests);
  }
  return errcount;
}
