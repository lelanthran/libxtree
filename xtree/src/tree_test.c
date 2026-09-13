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
  struct xtree_errobj_t err = {};

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
  struct xtree_errobj_t err = {};

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
  struct xtree_errobj_t err = {};

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
  struct xtree_errobj_t err = {};

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
  struct xtree_errobj_t err = {};

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
  struct xtree_errobj_t err = {};

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
  struct xtree_errobj_t err = {};

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
  struct xtree_errobj_t err = {};

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
  struct xtree_errobj_t err = {};

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
  struct xtree_errobj_t err = {};

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
  struct xtree_errobj_t err = {};

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
  struct xtree_errobj_t err = {};

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
  struct xtree_errobj_t err = {};

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
  struct xtree_errobj_t err = {};

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
  struct xtree_errobj_t err = {};

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
  struct xtree_errobj_t err = {};

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

// name: test_tree_dump
int test_tree_dump (void)
{
  int ret = EXIT_FAILURE;
  struct xtree_errobj_t err = {};

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

  ret = EXIT_SUCCESS;

cleanup:
  xtree_node_free (&root);
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
    TEST (test_tree_dump),
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

