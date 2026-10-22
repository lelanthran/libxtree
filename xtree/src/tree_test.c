#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include <unistd.h>


/* Small test program to test the xtree library */
#include "ds_str.h"
#include "xtree_err.h"
#include "xtree.h"
#include "xtree_reader.h"


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

/* Read an entire file into a single heap-allocated NUL-terminated
 * string. Caller must free the returned buffer. Returns NULL on
 * error. */
static char *fslurp (const char *fname)
{
  if (!fname)
    return NULL;

  FILE *f = fopen (fname, "rb");
  if (!f)
    return NULL;

  if (fseek (f, 0, SEEK_END) != 0) {
    fclose (f);
    return NULL;
  }

  long len = ftell (f);
  if (len < 0) {
    fclose (f);
    return NULL;
  }
  rewind (f);

  char *buf = malloc ((size_t)len + 1);
  if (!buf) {
    fclose (f);
    return NULL;
  }

  size_t got = fread (buf, 1, (size_t)len, f);
  fclose (f);

  if (got != (size_t)len) {
    free (buf);
    return NULL;
  }

  buf[(size_t)len] = '\0';
  return buf;
}

/* Create a temporary file under /tmp. On success *dst_fname is set to a
 * heap-allocated filename (caller must free) and *dst_outf to an open
 * FILE* (caller must fclose). Returns true on success, false on
 * error. */
static bool fdump (char **dst_fname, FILE **dst_outf)
{
  if (!dst_fname || !dst_outf)
    return false;

  *dst_fname = NULL;
  *dst_outf = NULL;

  char tmpl[] = "/tmp/xtree_dump_XXXXXX";
  int fd = mkstemp (tmpl);
  if (fd < 0)
    return false;

  FILE *outf = fdopen (fd, "w+");
  if (!outf) {
    close (fd);
    remove (tmpl);
    return false;
  }

  char *fname = ds_str_dup (tmpl);
  if (!fname) {
    fclose (outf);
    remove (tmpl);
    return false;
  }

  *dst_fname = fname;
  *dst_outf = outf;
  return true;
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
  FILE *outf = NULL;
  char *fname = NULL;
  char *actual = NULL;

  xtree_node_t *root = NULL;
  xtree_node_t *group = NULL;
  xtree_node_t *leaf1 = NULL;
  xtree_node_t *leaf2 = NULL;
  xtree_node_t *branch = NULL;
  xtree_node_t *empty = NULL;
  xtree_node_t *topatom = NULL;

  /* Expected dump, using the same format produced by xtree_node_dump:
   *
   *   - one space of indent per depth level
   *   - parent is printed as "No parent" for a root node, otherwise
   *     the parent's name (which may be the empty string)
   *   - attribute lines use the node's name (which may be the empty
   *     string) between "node:" and ":attr[...]"
   *
   * The tree under test is built below in the same order as this
   * expected string.
   */
  static const char *expected =
    "node:         (root)\n"
    "node:type     [2]\n"
    "node:parent   [No parent]\n"
    "node:nattrs     3\n"
    "node:root:attr[version:1.0]\n"
    "node:root:attr[id:r1]\n"
    "node:root:attr[id:r2]\n"
    "node:nchildren  3\n"
    " node:         ()\n"
    " node:type     [2]\n"
    " node:parent   [root]\n"
    " node:nattrs     3\n"
    " node::attr[kind:group]\n"
    " node::attr[k:v1]\n"
    " node::attr[k:v2]\n"
    " node:nchildren  2\n"
    "  node:         (leaf1)\n"
    "  node:type     [1]\n"
    "  node:parent   []\n"
    "  node:nattrs     3\n"
    "  node:leaf1:attr[id:l1]\n"
    "  node:leaf1:attr[tag:first]\n"
    "  node:leaf1:attr[tag:second]\n"
    "  node:value  [alpha]\n"
    "  node:         ()\n"
    "  node:type     [1]\n"
    "  node:parent   []\n"
    "  node:nattrs     0\n"
    "  node:value  [beta]\n"
    " node:         (branch)\n"
    " node:type     [2]\n"
    " node:parent   [root]\n"
    " node:nattrs     0\n"
    " node:nchildren  1\n"
    "  node:         (empty)\n"
    "  node:type     [2]\n"
    "  node:parent   [branch]\n"
    "  node:nattrs     0\n"
    "  node:nchildren  0\n"
    " node:         (topatom)\n"
    " node:type     [1]\n"
    " node:parent   [root]\n"
    " node:nattrs     1\n"
    " node:topatom:attr[id:t1]\n"
    " node:value  [delta]\n";

  if (!(xtree_errobj_reset (&err, 1024))) {
    PERROR ("Failed to allocate error object\n");
    goto cleanup;
  }

  /* Root: LIST "root" with a version attribute and two duplicate id
   * attributes, exercising ordered duplicate keys. */
  root = xtree_node_new (&err, NULL, "root", xtree_node_type_LIST);
  if (!root) {
    PERROR ("Failed to create root node\n");
    goto cleanup;
  }
  if (!xtree_node_attr_new (&err, root, "version", "1.0")) {
    PERROR ("Failed to add root attribute version\n");
    goto cleanup;
  }
  if (!xtree_node_attr_new (&err, root, "id", "r1")) {
    PERROR ("Failed to add root attribute id r1\n");
    goto cleanup;
  }
  if (!xtree_node_attr_new (&err, root, "id", "r2")) {
    PERROR ("Failed to add root attribute id r2\n");
    goto cleanup;
  }

  /* Anonymous intermediate LIST node with three attributes (including
   * a duplicate k key) and two children. */
  group = xtree_node_new (&err, root, NULL, xtree_node_type_LIST);
  if (!group) {
    PERROR ("Failed to create group node\n");
    goto cleanup;
  }
  if (!xtree_node_attr_new (&err, group, "kind", "group")) {
    PERROR ("Failed to add group attribute kind\n");
    goto cleanup;
  }
  if (!xtree_node_attr_new (&err, group, "k", "v1")) {
    PERROR ("Failed to add group attribute k v1\n");
    goto cleanup;
  }
  if (!xtree_node_attr_new (&err, group, "k", "v2")) {
    PERROR ("Failed to add group attribute k v2\n");
    goto cleanup;
  }

  /* Named ATOM with a value and duplicate tag attributes. */
  leaf1 = xtree_node_new (&err, group, "leaf1", xtree_node_type_ATOM);
  if (!leaf1) {
    PERROR ("Failed to create leaf1 node\n");
    goto cleanup;
  }
  if (!xtree_node_value_set (&err, leaf1, "alpha")) {
    PERROR ("Failed to set leaf1 value\n");
    goto cleanup;
  }
  if (!xtree_node_attr_new (&err, leaf1, "id", "l1")) {
    PERROR ("Failed to add leaf1 attribute id\n");
    goto cleanup;
  }
  if (!xtree_node_attr_new (&err, leaf1, "tag", "first")) {
    PERROR ("Failed to add leaf1 attribute tag first\n");
    goto cleanup;
  }
  if (!xtree_node_attr_new (&err, leaf1, "tag", "second")) {
    PERROR ("Failed to add leaf1 attribute tag second\n");
    goto cleanup;
  }

  /* Anonymous ATOM with a value but no attributes. */
  leaf2 = xtree_node_new (&err, group, NULL, xtree_node_type_ATOM);
  if (!leaf2) {
    PERROR ("Failed to create leaf2 node\n");
    goto cleanup;
  }
  if (!xtree_node_value_set (&err, leaf2, "beta")) {
    PERROR ("Failed to set leaf2 value\n");
    goto cleanup;
  }

  /* LIST with no attributes that contains a single empty LIST child. */
  branch = xtree_node_new (&err, root, "branch", xtree_node_type_LIST);
  if (!branch) {
    PERROR ("Failed to create branch node\n");
    goto cleanup;
  }

  empty = xtree_node_new (&err, branch, "empty", xtree_node_type_LIST);
  if (!empty) {
    PERROR ("Failed to create empty node\n");
    goto cleanup;
  }

  /* Named ATOM sibling with a value and a single attribute. */
  topatom = xtree_node_new (&err, root, "topatom", xtree_node_type_ATOM);
  if (!topatom) {
    PERROR ("Failed to create topatom node\n");
    goto cleanup;
  }
  if (!xtree_node_value_set (&err, topatom, "delta")) {
    PERROR ("Failed to set topatom value\n");
    goto cleanup;
  }
  if (!xtree_node_attr_new (&err, topatom, "id", "t1")) {
    PERROR ("Failed to add topatom attribute id\n");
    goto cleanup;
  }

  /* Dump to a temp file under /tmp, slurp it back, compare. */
  if (!fdump (&fname, &outf)) {
    PERROR ("Failed to create temporary dump file\n");
    goto cleanup;
  }

  xtree_node_dump (root, outf, 0);

  if (fclose (outf) != 0) {
    PERROR ("Failed to close temporary dump file\n");
    outf = NULL;
    goto cleanup;
  }
  outf = NULL;

  actual = fslurp (fname);
  if (!actual) {
    PERROR ("Failed to read temporary dump file [%s]\n", fname);
    goto cleanup;
  }

  if (strcmp (actual, expected) != 0) {
    PERROR ("Dump does not match expected output\n"
            "--- expected ---\n%s"
            "--- actual ---\n%s",
            expected, actual);
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  if (outf)
    fclose (outf);
  if (fname) {
    remove (fname);
    free (fname);
  }
  free (actual);
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

/* ------------------------------------------------------------------
 * Tests for xtree_reader_t
 *
 * Notes on the assumptions made by these tests:
 *
 *   - xtree_reader_get() and xtree_reader_peek() return EOF when there
 *     is nothing left to read.
 *   - The absolute starting values of xtree_reader_line() and
 *     xtree_reader_cpos() are not assumed (0-based or 1-based); the
 *     tests record the starting values and check relative changes.
 *   - There is no whitespace handling in the reader: every character of
 *     the input, whitespace included, is returned as-is by get and peek.
 *   - A newline moves the reader to the next line and resets the
 *     character position to its starting value.
 *   - xtree_reader_unget() only works once: it is never called twice
 *     without an intervening xtree_reader_get() (or before the first
 *     get), as that behaviour is undefined.
 *   - xtree_reader_error() returns NULL when the reader has no error,
 *     and otherwise a pointer to a read-only xtree_errobj_t that is
 *     owned by the reader (the caller does not free or reset it).
 * ------------------------------------------------------------------ */

/* Check that the next character returned by the reader is `expected`.
 * Returns true on a match, false (after reporting the mismatch)
 * otherwise. */
static bool rdr_expect_get (xtree_reader_t *reader, int expected)
{
  int got = xtree_reader_get (reader);
  if (got != expected) {
    PERROR ("Expected get to return [%i], got [%i]\n", expected, got);
    return false;
  }
  return true;
}

/* Report every field of an error object returned by the reader. */
static void rdr_print_errobj (const struct xtree_errobj_t *err)
{
  PERROR ("errobj: libcode=%i libmsg=[%.*s] syscode=%i sysmsg=[%.*s] "
          "src=[%.*s:%i] message=[%s] message_len=%zu\n",
          (int)err->libcode, (int)sizeof err->libmsg, err->libmsg,
          err->syscode, (int)sizeof err->sysmsg, err->sysmsg,
          (int)sizeof err->srcfile, err->srcfile, err->srcline,
          err->message ? err->message : "", err->message_len);
}

/* Check that the reader has no error: xtree_reader_error() must return
 * NULL. If it does not, the contents of the error object are reported.
 * Returns true if there is no error. */
static bool rdr_expect_noerror (const xtree_reader_t *reader)
{
  const struct xtree_errobj_t *err = xtree_reader_error (reader);
  if (err) {
    PERROR ("Reader should not have an error object\n");
    rdr_print_errobj (err);
    return false;
  }
  return true;
}

/* Check that an error object returned by the reader is well-formed: it
 * records an error (libcode or syscode set), the fixed-size strings are
 * all NUL-terminated within their buffers, and the dynamic message, if
 * present, is NUL-terminated within message_len. Returns true if the
 * object is well-formed. */
static bool rdr_expect_wellformed_errobj (const struct xtree_errobj_t *err)
{
  if (!err) {
    PERROR ("Error object is NULL\n");
    return false;
  }
  if (!err->libcode && !err->syscode) {
    PERROR ("Error object is set but records neither a libcode nor a "
            "syscode\n");
    rdr_print_errobj (err);
    return false;
  }
  if (!memchr (err->sysmsg, '\0', sizeof err->sysmsg) ||
      !memchr (err->libmsg, '\0', sizeof err->libmsg) ||
      !memchr (err->srcfile, '\0', sizeof err->srcfile)) {
    PERROR ("Error object has an unterminated fixed-size string\n");
    return false;
  }
  if (err->message && !memchr (err->message, '\0', err->message_len)) {
    PERROR ("Error object message is not terminated within message_len\n");
    return false;
  }
  return true;
}

// name: test_reader_create_free
int test_reader_create_free (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;

  reader = xtree_reader_str ("test", "abc");
  if (!reader) {
    PERROR ("Failed to create reader from string\n");
    goto cleanup;
  }

  if (!rdr_expect_noerror (reader))
    goto cleanup;
  if (xtree_reader_eof (reader)) {
    PERROR ("Reader over non-empty input must not start at EOF\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  return ret;
}

// name: test_reader_free_null_safety
int test_reader_free_null_safety (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;

  reader = xtree_reader_str ("test", "abc");
  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  // Test standard free
  xtree_reader_free (&reader);
  if (reader != NULL) {
    PERROR ("xtree_reader_free should set the pointer to NULL\n");
    goto cleanup;
  }

  // Test freeing NULL pointer (should not crash)
  xtree_reader_free (&reader);
  xtree_reader_free (NULL);

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  return ret;
}

// name: test_reader_str_null_input
int test_reader_str_null_input (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;

  reader = xtree_reader_str ("test", NULL);
  if (reader != NULL) {
    PERROR ("Creating a reader from a NULL string should return NULL\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  return ret;
}

// name: test_reader_empty_input
int test_reader_empty_input (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;

  reader = xtree_reader_str ("test", "");
  if (!reader) {
    PERROR ("Failed to create reader from the empty string\n");
    goto cleanup;
  }

  if (xtree_reader_peek (reader) != EOF) {
    PERROR ("Peek on empty input should return EOF\n");
    goto cleanup;
  }
  if (!rdr_expect_get (reader, EOF))
    goto cleanup;

  if (!xtree_reader_eof (reader)) {
    PERROR ("Reader should report EOF after reading empty input\n");
    goto cleanup;
  }
  // Reaching EOF is not an error.
  if (!rdr_expect_noerror (reader))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  return ret;
}

// name: test_reader_get_sequence
int test_reader_get_sequence (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;

  reader = xtree_reader_str ("test", "abc");
  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  if (!rdr_expect_get (reader, 'a'))
    goto cleanup;
  if (!rdr_expect_get (reader, 'b'))
    goto cleanup;
  if (!rdr_expect_get (reader, 'c'))
    goto cleanup;
  if (!rdr_expect_get (reader, EOF))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  return ret;
}

// name: test_reader_eof_transitions
int test_reader_eof_transitions (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;

  reader = xtree_reader_str ("test", "x");
  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  if (xtree_reader_eof (reader)) {
    PERROR ("Reader must not be at EOF before reading anything\n");
    goto cleanup;
  }

  if (!rdr_expect_get (reader, 'x'))
    goto cleanup;

  // Reading past the end returns EOF and sets the EOF state.
  if (!rdr_expect_get (reader, EOF))
    goto cleanup;
  if (!xtree_reader_eof (reader)) {
    PERROR ("Reader should report EOF after reading past the end\n");
    goto cleanup;
  }

  // EOF is sticky: further reads keep returning EOF without erroring.
  if (!rdr_expect_get (reader, EOF))
    goto cleanup;
  if (!xtree_reader_eof (reader)) {
    PERROR ("EOF state should persist on repeated reads\n");
    goto cleanup;
  }
  // Reaching EOF must not set the error object.
  if (!rdr_expect_noerror (reader))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  return ret;
}

// name: test_reader_peek_no_advance
int test_reader_peek_no_advance (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;

  reader = xtree_reader_str ("test", "xy");
  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  size_t line = xtree_reader_line (reader);
  size_t cpos = xtree_reader_cpos (reader);

  // Peeking repeatedly returns the same character...
  if (xtree_reader_peek (reader) != 'x') {
    PERROR ("First peek should return [x]\n");
    goto cleanup;
  }
  if (xtree_reader_peek (reader) != 'x') {
    PERROR ("Second peek should still return [x]\n");
    goto cleanup;
  }

  // ...and does not move the position.
  if (xtree_reader_line (reader) != line ||
      xtree_reader_cpos (reader) != cpos) {
    PERROR ("Peek must not change the line number or character position\n");
    goto cleanup;
  }

  // The peeked character is then the one that get returns.
  if (!rdr_expect_get (reader, 'x'))
    goto cleanup;
  if (xtree_reader_peek (reader) != 'y') {
    PERROR ("Peek after get should return [y]\n");
    goto cleanup;
  }
  if (!rdr_expect_get (reader, 'y'))
    goto cleanup;

  // Peeking at the end returns EOF.
  if (xtree_reader_peek (reader) != EOF) {
    PERROR ("Peek at the end of input should return EOF\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  return ret;
}

// name: test_reader_whitespace_returned
int test_reader_whitespace_returned (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;

  reader = xtree_reader_str ("test", " \t\r\n x ");
  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  // Every whitespace character is significant and is returned as-is.
  if (!rdr_expect_get (reader, ' '))
    goto cleanup;
  if (!rdr_expect_get (reader, '\t'))
    goto cleanup;
  if (!rdr_expect_get (reader, '\r'))
    goto cleanup;
  if (!rdr_expect_get (reader, '\n'))
    goto cleanup;
  if (!rdr_expect_get (reader, ' '))
    goto cleanup;
  if (!rdr_expect_get (reader, 'x'))
    goto cleanup;
  if (!rdr_expect_get (reader, ' '))
    goto cleanup;
  if (!rdr_expect_get (reader, EOF))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  return ret;
}

// name: test_reader_whitespace_only_input
int test_reader_whitespace_only_input (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;

  reader = xtree_reader_str ("test", "  \n\t");
  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  // Whitespace-only input is returned character by character; it is
  // not mistaken for an empty input.
  if (!rdr_expect_get (reader, ' '))
    goto cleanup;
  if (!rdr_expect_get (reader, ' '))
    goto cleanup;
  if (!rdr_expect_get (reader, '\n'))
    goto cleanup;
  if (!rdr_expect_get (reader, '\t'))
    goto cleanup;
  if (!rdr_expect_get (reader, EOF))
    goto cleanup;
  if (!xtree_reader_eof (reader)) {
    PERROR ("Reader should be at EOF after reading all whitespace\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  return ret;
}

// name: test_reader_peek_whitespace
int test_reader_peek_whitespace (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;

  reader = xtree_reader_str ("test", " x");
  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  size_t line = xtree_reader_line (reader);
  size_t cpos = xtree_reader_cpos (reader);

  // Whitespace is returned by peek just like any other character, and
  // peeking does not move the position.
  if (xtree_reader_peek (reader) != ' ') {
    PERROR ("Peek should return the leading space\n");
    goto cleanup;
  }
  if (xtree_reader_line (reader) != line ||
      xtree_reader_cpos (reader) != cpos) {
    PERROR ("Peek must not change the position\n");
    goto cleanup;
  }

  if (!rdr_expect_get (reader, ' '))
    goto cleanup;
  if (!rdr_expect_get (reader, 'x'))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  return ret;
}

// name: test_reader_line_tracking
int test_reader_line_tracking (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;

  reader = xtree_reader_str ("test", "a\nb\n\nc");
  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  // Each newline is returned and advances the line by one.
  size_t start = xtree_reader_line (reader);

  if (!rdr_expect_get (reader, 'a'))
    goto cleanup;
  if (xtree_reader_line (reader) != start) {
    PERROR ("Line should not change after reading [a]\n");
    goto cleanup;
  }
  if (!rdr_expect_get (reader, '\n'))
    goto cleanup;
  if (xtree_reader_line (reader) != start + 1) {
    PERROR ("Line should be %zu after the first newline, got %zu\n",
            start + 1, xtree_reader_line (reader));
    goto cleanup;
  }
  if (!rdr_expect_get (reader, 'b'))
    goto cleanup;
  if (!rdr_expect_get (reader, '\n'))
    goto cleanup;
  if (!rdr_expect_get (reader, '\n'))
    goto cleanup;
  if (!rdr_expect_get (reader, 'c'))
    goto cleanup;
  if (xtree_reader_line (reader) != start + 3) {
    PERROR ("Line should be %zu after reading everything, got %zu\n",
            start + 3, xtree_reader_line (reader));
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  return ret;
}

// name: test_reader_cpos_tracking
int test_reader_cpos_tracking (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;
  xtree_reader_t *multi = NULL;

  reader = xtree_reader_str ("test", "abc");
  multi = xtree_reader_str ("test", "ab\ncd");
  if (!reader || !multi) {
    PERROR ("Failed to create readers\n");
    goto cleanup;
  }

  // Each character read advances the character position by one.
  size_t start = xtree_reader_cpos (reader);
  for (size_t i=0; i<3; i++) {
    if (xtree_reader_cpos (reader) != start + i) {
      PERROR ("Before reading character %zu: expected cpos %zu, got %zu\n",
              i, start + i, xtree_reader_cpos (reader));
      goto cleanup;
    }
    if (xtree_reader_get (reader) == EOF) {
      PERROR ("Unexpected EOF at character %zu\n", i);
      goto cleanup;
    }
  }
  if (xtree_reader_cpos (reader) != start + 3) {
    PERROR ("After reading 3 characters: expected cpos %zu, got %zu\n",
            start + 3, xtree_reader_cpos (reader));
    goto cleanup;
  }

  // A newline resets the character position.
  start = xtree_reader_cpos (multi);
  if (!rdr_expect_get (multi, 'a'))
    goto cleanup;
  if (!rdr_expect_get (multi, 'b'))
    goto cleanup;
  if (xtree_reader_cpos (multi) != start + 2) {
    PERROR ("Expected cpos %zu before the newline, got %zu\n",
            start + 2, xtree_reader_cpos (multi));
    goto cleanup;
  }
  if (!rdr_expect_get (multi, '\n'))
    goto cleanup;
  if (xtree_reader_cpos (multi) != start) {
    PERROR ("Newline should reset cpos to %zu, got %zu\n",
            start, xtree_reader_cpos (multi));
    goto cleanup;
  }
  if (!rdr_expect_get (multi, 'c'))
    goto cleanup;
  if (xtree_reader_cpos (multi) != start + 1) {
    PERROR ("Expected cpos %zu after [c], got %zu\n",
            start + 1, xtree_reader_cpos (multi));
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  xtree_reader_free (&multi);
  return ret;
}

// name: test_reader_unget_basic
int test_reader_unget_basic (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;

  reader = xtree_reader_str ("test", "ab");
  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  if (!rdr_expect_get (reader, 'a'))
    goto cleanup;

  xtree_reader_unget (reader);
  if (xtree_reader_peek (reader) != 'a') {
    PERROR ("Peek after unget should return the ungotten [a]\n");
    goto cleanup;
  }
  if (!rdr_expect_get (reader, 'a'))
    goto cleanup;
  if (!rdr_expect_get (reader, 'b'))
    goto cleanup;

  xtree_reader_unget (reader);
  if (!rdr_expect_get (reader, 'b'))
    goto cleanup;
  if (!rdr_expect_get (reader, EOF))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  return ret;
}

// name: test_reader_unget_repeated_cycles
int test_reader_unget_repeated_cycles (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;
  static const char *input = "abc";

  reader = xtree_reader_str ("test", input);
  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  // Unget only works once without an intervening get, but it may be used
  // again after each get: every character can be read, ungotten and read
  // again, all the way through the input.
  for (size_t i=0; input[i]; i++) {
    if (!rdr_expect_get (reader, input[i]))
      goto cleanup;
    xtree_reader_unget (reader);
    if (!rdr_expect_get (reader, input[i]))
      goto cleanup;
  }
  if (!rdr_expect_get (reader, EOF))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  return ret;
}

// name: test_reader_unget_restores_position
int test_reader_unget_restores_position (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;
  const int expected[3] = { 'a', '\n', 'b' };
  size_t lines[3];
  size_t cposs[3];

  reader = xtree_reader_str ("test", "a\nb");
  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  // For each character: record the position, get it, unget it, and check
  // that the position is restored. This includes stepping back across the
  // newline. Unget is only used once per get.
  for (size_t i=0; i<3; i++) {
    lines[i] = xtree_reader_line (reader);
    cposs[i] = xtree_reader_cpos (reader);

    if (!rdr_expect_get (reader, expected[i]))
      goto cleanup;

    xtree_reader_unget (reader);
    if (xtree_reader_line (reader) != lines[i]) {
      PERROR ("Unget after character %zu: expected line %zu, got %zu\n",
              i, lines[i], xtree_reader_line (reader));
      goto cleanup;
    }
    if (xtree_reader_cpos (reader) != cposs[i]) {
      PERROR ("Unget after character %zu: expected cpos %zu, got %zu\n",
              i, cposs[i], xtree_reader_cpos (reader));
      goto cleanup;
    }

    // Re-reading must reproduce the same character, and leave the
    // position where a plain get would have left it.
    if (!rdr_expect_get (reader, expected[i]))
      goto cleanup;
  }

  // After the re-reads the reader must be at the end of the input, on the
  // second line.
  if (xtree_reader_line (reader) != lines[2]) {
    PERROR ("Expected line %zu at the end, got %zu\n",
            lines[2], xtree_reader_line (reader));
    goto cleanup;
  }
  if (xtree_reader_cpos (reader) != cposs[2] + 1) {
    PERROR ("Expected cpos %zu at the end, got %zu\n",
            cposs[2] + 1, xtree_reader_cpos (reader));
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  return ret;
}

// name: test_reader_unget_whitespace
int test_reader_unget_whitespace (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;

  reader = xtree_reader_str ("test", "a b\nc");
  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  // Whitespace characters can be ungotten like any other, including a
  // newline, which must restore the line number and character position.
  if (!rdr_expect_get (reader, 'a'))
    goto cleanup;
  if (!rdr_expect_get (reader, ' '))
    goto cleanup;

  xtree_reader_unget (reader);
  if (xtree_reader_peek (reader) != ' ') {
    PERROR ("Peek after unget should return the ungotten space\n");
    goto cleanup;
  }
  if (!rdr_expect_get (reader, ' '))
    goto cleanup;
  if (!rdr_expect_get (reader, 'b'))
    goto cleanup;

  size_t line = xtree_reader_line (reader);
  size_t cpos = xtree_reader_cpos (reader);

  if (!rdr_expect_get (reader, '\n'))
    goto cleanup;
  if (xtree_reader_line (reader) != line + 1) {
    PERROR ("Newline should advance the line to %zu, got %zu\n",
            line + 1, xtree_reader_line (reader));
    goto cleanup;
  }

  xtree_reader_unget (reader);
  if (xtree_reader_line (reader) != line ||
      xtree_reader_cpos (reader) != cpos) {
    PERROR ("Unget of newline should restore line %zu cpos %zu, "
            "got line %zu cpos %zu\n", line, cpos,
            xtree_reader_line (reader), xtree_reader_cpos (reader));
    goto cleanup;
  }
  if (!rdr_expect_get (reader, '\n'))
    goto cleanup;
  if (!rdr_expect_get (reader, 'c'))
    goto cleanup;
  if (!rdr_expect_get (reader, EOF))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  return ret;
}

// name: test_reader_input_copied
int test_reader_input_copied (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;
  char buf[] = "abc";

  reader = xtree_reader_str ("test", buf);
  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  // The reader owns a copy, so mutating the caller's buffer afterwards
  // must not affect what the reader returns.
  memset (buf, 'Z', sizeof buf - 1);

  if (!rdr_expect_get (reader, 'a'))
    goto cleanup;
  if (!rdr_expect_get (reader, 'b'))
    goto cleanup;
  if (!rdr_expect_get (reader, 'c'))
    goto cleanup;
  if (!rdr_expect_get (reader, EOF))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  return ret;
}

// name: test_reader_independent_readers
int test_reader_independent_readers (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *r1 = NULL;
  xtree_reader_t *r2 = NULL;

  r1 = xtree_reader_str ("one", "abc");
  r2 = xtree_reader_str ("two", "abc");
  if (!r1 || !r2) {
    PERROR ("Failed to create readers\n");
    goto cleanup;
  }

  size_t start = xtree_reader_cpos (r2);

  if (!rdr_expect_get (r1, 'a'))
    goto cleanup;
  if (!rdr_expect_get (r1, 'b'))
    goto cleanup;

  // r2 must be unaffected by reads on r1.
  if (xtree_reader_cpos (r2) != start) {
    PERROR ("Reading from one reader changed the position of another\n");
    goto cleanup;
  }
  if (!rdr_expect_get (r2, 'a'))
    goto cleanup;
  if (!rdr_expect_get (r1, 'c'))
    goto cleanup;
  if (!rdr_expect_get (r2, 'b'))
    goto cleanup;

  // Freeing one must not disturb the other.
  xtree_reader_free (&r1);
  if (!rdr_expect_get (r2, 'c'))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&r1);
  xtree_reader_free (&r2);
  return ret;
}

// name: test_reader_error_none_on_success
int test_reader_error_none_on_success (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;
  const struct xtree_errobj_t *err = NULL;

  reader = xtree_reader_str ("test", "a b\nc");
  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  // No error object on a fresh reader...
  err = xtree_reader_error (reader);
  if (err != NULL) {
    PERROR ("Fresh reader should return a NULL error object\n");
    rdr_print_errobj (err);
    goto cleanup;
  }

  // ...nor after any of the normal operations.
  if (!rdr_expect_get (reader, 'a'))
    goto cleanup;
  if (!rdr_expect_noerror (reader))
    goto cleanup;

  if (xtree_reader_peek (reader) != ' ') {
    PERROR ("Peek should return the space\n");
    goto cleanup;
  }
  if (!rdr_expect_noerror (reader))
    goto cleanup;

  // Unget requires a preceding get, which was done above.
  xtree_reader_unget (reader);
  if (!rdr_expect_noerror (reader))
    goto cleanup;

  while (xtree_reader_get (reader) != EOF)
    ;
  if (!rdr_expect_noerror (reader))
    goto cleanup;

  // Neither does reading again after EOF.
  if (!rdr_expect_get (reader, EOF))
    goto cleanup;
  if (!rdr_expect_noerror (reader))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  return ret;
}

// name: test_reader_error_query_idempotent
int test_reader_error_query_idempotent (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;

  reader = xtree_reader_str ("test", "ab");
  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  if (!rdr_expect_get (reader, 'a'))
    goto cleanup;

  size_t line = xtree_reader_line (reader);
  size_t cpos = xtree_reader_cpos (reader);

  // Querying the error (any number of times) is a pure read: it must
  // not change the result of the query or the position of the reader.
  for (size_t i=0; i<3; i++) {
    if (!rdr_expect_noerror (reader))
      goto cleanup;
  }

  if (xtree_reader_line (reader) != line ||
      xtree_reader_cpos (reader) != cpos) {
    PERROR ("Querying the error must not change the reader position\n");
    goto cleanup;
  }
  if (!rdr_expect_get (reader, 'b'))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  return ret;
}

// name: test_reader_error_null_reader
int test_reader_error_null_reader (void)
{
  // A NULL reader must be handled without crashing. What it reports is
  // up to the library: either NULL, or a well-formed error object.
  const struct xtree_errobj_t *err = xtree_reader_error (NULL);
  if (err && !rdr_expect_wellformed_errobj (err))
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}

// name: test_reader_roundtrip
int test_reader_roundtrip (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;
  static const char *input = "foo bar\n  baz\tqux\n";
  char out[64];
  size_t n = 0;
  int c;

  // The reader reproduces its input exactly.
  reader = xtree_reader_str ("test", input);
  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  while ((c = xtree_reader_get (reader)) != EOF) {
    if (n + 1 >= sizeof out) {
      PERROR ("Reader returned more characters than the input holds\n");
      goto cleanup;
    }
    out[n++] = (char)c;
  }
  out[n] = '\0';

  if (strcmp (out, input) != 0) {
    PERROR ("Roundtrip mismatch, got [%s]\n", out);
    goto cleanup;
  }
  if (!xtree_reader_eof (reader)) {
    PERROR ("Reader should be at EOF after the roundtrip\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  return ret;
}

int main (void)
{
  size_t errcount = 0;
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
    /* Tests for xtree_reader_t */
    TEST (test_reader_create_free),
    TEST (test_reader_free_null_safety),
    TEST (test_reader_str_null_input),
    TEST (test_reader_empty_input),
    TEST (test_reader_get_sequence),
    TEST (test_reader_eof_transitions),
    TEST (test_reader_peek_no_advance),
    TEST (test_reader_whitespace_returned),
    TEST (test_reader_whitespace_only_input),
    TEST (test_reader_peek_whitespace),
    TEST (test_reader_line_tracking),
    TEST (test_reader_cpos_tracking),
    TEST (test_reader_unget_basic),
    TEST (test_reader_unget_repeated_cycles),
    TEST (test_reader_unget_restores_position),
    TEST (test_reader_unget_whitespace),
    TEST (test_reader_input_copied),
    TEST (test_reader_independent_readers),
    TEST (test_reader_error_none_on_success),
    TEST (test_reader_error_query_idempotent),
    TEST (test_reader_error_null_reader),
    TEST (test_reader_roundtrip),
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
    printf ("FAILED: %zu/%zu test failed\n", errcount, ntests);
  } else {
    printf ("PASSED: %zu tests passed\n", ntests);
  }
  return (int)errcount;
}
