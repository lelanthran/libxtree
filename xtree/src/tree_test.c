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
#include "xtree_scan.h"
#include "xtree_json_scan.h"


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

// name: test_reader_high_bytes
int test_reader_high_bytes (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;

  // Bytes with the high bit set (as found in UTF-8 text) must be returned
  // as values in the range 0..255, never as negative values. In
  // particular, a 0xFF byte must not be confused with EOF.
  static const char input[] = {
    (char)0xff, (char)0xc3, (char)0xa9, (char)0x80, 'z', '\0'
  };
  static const int expected[] = { 0xff, 0xc3, 0xa9, 0x80, 'z' };

  reader = xtree_reader_str ("test", input);
  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  for (size_t i=0; i<sizeof expected / sizeof expected[0]; i++) {
    int p = xtree_reader_peek (reader);
    if (p != expected[i]) {
      PERROR ("Character %zu: expected peek to return [%i], got [%i]\n",
              i, expected[i], p);
      goto cleanup;
    }
    if (!rdr_expect_get (reader, expected[i]))
      goto cleanup;
    if (!rdr_expect_noerror (reader))
      goto cleanup;
  }

  if (!rdr_expect_get (reader, EOF))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  return ret;
}

// name: test_reader_unget_after_eof
int test_reader_unget_after_eof (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;
  xtree_reader_t *empty = NULL;

  reader = xtree_reader_str ("test", "ab");
  empty = xtree_reader_str ("test", "");
  if (!reader || !empty) {
    PERROR ("Failed to create readers\n");
    goto cleanup;
  }

  if (!rdr_expect_get (reader, 'a'))
    goto cleanup;
  if (!rdr_expect_get (reader, 'b'))
    goto cleanup;

  size_t line = xtree_reader_line (reader);
  size_t cpos = xtree_reader_cpos (reader);

  // A get that returns EOF consumed nothing, so unget must not give back
  // a character: the next get must return EOF again and the position
  // must be unchanged.
  if (!rdr_expect_get (reader, EOF))
    goto cleanup;
  // A get after seeing EOF must do nothing; we should be able to call get
  // repeatedly and expect EOF on each call.
  if (!rdr_expect_get (reader, EOF))
    goto cleanup;


  // This ungets "b", setting the character position to 1
  xtree_reader_unget (reader);

  if (xtree_reader_line (reader) != line ||
      xtree_reader_cpos (reader) != cpos) {
    PERROR ("Unget after EOF changed the position: expected line %zu cpos "
            "%zu, got line %zu cpos %zu\n", line, cpos,
            xtree_reader_line (reader), xtree_reader_cpos (reader));
    goto cleanup;
  }
  if (xtree_reader_peek (reader) != EOF) {
    PERROR ("Peek after unget at EOF should still return EOF\n");
    goto cleanup;
  }
  if (!rdr_expect_get (reader, EOF))
    goto cleanup;

  // The same holds for input that is empty to begin with.
  line = xtree_reader_line (empty);
  cpos = xtree_reader_cpos (empty);

  if (!rdr_expect_get (empty, EOF))
    goto cleanup;
  xtree_reader_unget (empty);
  if (!rdr_expect_get (empty, EOF))
    goto cleanup;
  if (xtree_reader_line (empty) != line ||
      xtree_reader_cpos (empty) != cpos) {
    PERROR ("Unget after EOF on empty input changed the position\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  xtree_reader_free (&empty);
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

/* ------------------------------------------------------------------
 * Tests for xtree_reader_file()
 *
 * Assumptions:
 *   - xtree_reader_file() does NOT take ownership of the FILE*: the test
 *     closes it itself, after freeing the reader.
 *   - The FILE* stays open for the whole life of the reader (the reader
 *     may read lazily).
 *   - Behaviour must be identical to xtree_reader_str() for the same data.
 *
 * Add to the includes:   (nothing new; fdump() already pulls in what we need)
 * Add to the TEST list:  test_reader_file_basic,
 *                        test_reader_file_large,
 *                        test_reader_file_null_input
 * ------------------------------------------------------------------ */

/* Write `len` bytes of `data` to a new temporary file (using fdump() for
 * the name), close it, and return the filename in *dst_fname (caller
 * must remove() and free() it). Returns true on success. */
static bool rdr_write_tmpfile (const char *data, size_t len, char **dst_fname)
{
  FILE *outf = NULL;
  char *fname = NULL;

  if (!fdump (&fname, &outf)) {
    PERROR ("Failed to create temporary file\n");
    return false;
  }

  if (len && fwrite (data, 1, len, outf) != len) {
    PERROR ("Failed to write test data to [%s]\n", fname);
    fclose (outf);
    remove (fname);
    free (fname);
    return false;
  }

  if (fclose (outf) != 0) {
    PERROR ("Failed to close [%s]\n", fname);
    remove (fname);
    free (fname);
    return false;
  }

  *dst_fname = fname;
  return true;
}

// name: test_reader_file_basic
int test_reader_file_basic (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;
  char *fname = NULL;
  FILE *inf = NULL;

  /* Includes whitespace, blank lines and high bytes (0xff must not be
   * confused with EOF). */
  static const char data[] = { 'a', 'b', '\n', 'c', ' ', '\n', '\n',
                               (char)0xff, '\t', 'z' };
  static const int expected[] = { 'a', 'b', '\n', 'c', ' ', '\n', '\n',
                                  0xff, '\t', 'z' };
  static const size_t n = sizeof expected / sizeof expected[0];

  if (!rdr_write_tmpfile (data, sizeof data, &fname))
    goto cleanup;

  inf = fopen (fname, "rb");
  if (!inf) {
    PERROR ("Failed to reopen [%s] for reading\n", fname);
    goto cleanup;
  }

  reader = xtree_reader_file ("test", inf);
  if (!reader) {
    PERROR ("Failed to create reader from file\n");
    goto cleanup;
  }
  if (!rdr_expect_noerror (reader))
    goto cleanup;
  if (xtree_reader_eof (reader)) {
    PERROR ("Reader over a non-empty file must not start at EOF\n");
    goto cleanup;
  }

  size_t line0 = xtree_reader_line (reader);
  size_t cpos0 = xtree_reader_cpos (reader);
  size_t nl = 0;

  for (size_t i=0; i<n; i++) {
    // peek must agree with get and must not move the position
    size_t line = xtree_reader_line (reader);
    size_t cpos = xtree_reader_cpos (reader);
    int p = xtree_reader_peek (reader);
    if (p != expected[i]) {
      PERROR ("Character %zu: expected peek [%i], got [%i]\n",
              i, expected[i], p);
      goto cleanup;
    }
    if (xtree_reader_line (reader) != line ||
        xtree_reader_cpos (reader) != cpos) {
      PERROR ("Peek changed the position at character %zu\n", i);
      goto cleanup;
    }

    if (!rdr_expect_get (reader, expected[i]))
      goto cleanup;
    if (expected[i] == '\n')
      nl++;

    // unget/get round trip restores and then reproduces the position
    xtree_reader_unget (reader);
    if (xtree_reader_line (reader) != line ||
        xtree_reader_cpos (reader) != cpos) {
      PERROR ("Unget did not restore the position at character %zu\n", i);
      goto cleanup;
    }
    if (!rdr_expect_get (reader, expected[i]))
      goto cleanup;

    if (!rdr_expect_noerror (reader))
      goto cleanup;
  }

  // Line/cpos after consuming everything: three newlines were read, and
  // the last line holds 0xff, '\t', 'z'.
  if (xtree_reader_line (reader) != line0 + nl) {
    PERROR ("Expected line %zu at the end, got %zu\n",
            line0 + nl, xtree_reader_line (reader));
    goto cleanup;
  }
  if (xtree_reader_cpos (reader) != cpos0 + 3) {
    PERROR ("Expected cpos %zu at the end, got %zu\n",
            cpos0 + 3, xtree_reader_cpos (reader));
    goto cleanup;
  }

  // EOF, and it is sticky and not an error.
  if (xtree_reader_peek (reader) != EOF) {
    PERROR ("Peek at the end of the file should return EOF\n");
    goto cleanup;
  }
  if (!rdr_expect_get (reader, EOF))
    goto cleanup;
  if (!xtree_reader_eof (reader)) {
    PERROR ("Reader should report EOF after reading the whole file\n");
    goto cleanup;
  }
  if (!rdr_expect_get (reader, EOF))
    goto cleanup;
  if (!rdr_expect_noerror (reader))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  if (inf)
    fclose (inf);
  if (fname) {
    remove (fname);
    free (fname);
  }
  return ret;
}

// name: test_reader_file_large
int test_reader_file_large (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;
  char *fname = NULL;
  char *data = NULL;
  FILE *inf = NULL;

  /* Larger than any plausible internal buffer, so that refills are
   * exercised. Every 80th byte is a newline. */
  const size_t len = 100000;
  size_t nl = 0;

  data = malloc (len);
  if (!data) {
    PERROR ("Failed to allocate test data\n");
    goto cleanup;
  }
  for (size_t i=0; i<len; i++) {
    if (i % 80 == 79) {
      data[i] = '\n';
      nl++;
    } else {
      data[i] = (char)('a' + (i % 26));
    }
  }

  if (!rdr_write_tmpfile (data, len, &fname))
    goto cleanup;

  inf = fopen (fname, "rb");
  if (!inf) {
    PERROR ("Failed to reopen [%s] for reading\n", fname);
    goto cleanup;
  }

  reader = xtree_reader_file ("test", inf);
  if (!reader) {
    PERROR ("Failed to create reader from file\n");
    goto cleanup;
  }

  size_t line0 = xtree_reader_line (reader);

  for (size_t i=0; i<len; i++) {
    int c = xtree_reader_get (reader);
    if (c != (unsigned char)data[i]) {
      PERROR ("Byte %zu: expected [%i], got [%i]\n",
              i, (unsigned char)data[i], c);
      goto cleanup;
    }
    // Exercise unget across (likely) buffer boundaries too.
    if (i % 4093 == 0) {
      xtree_reader_unget (reader);
      if (!rdr_expect_get (reader, (unsigned char)data[i]))
        goto cleanup;
    }
  }

  if (xtree_reader_line (reader) != line0 + nl) {
    PERROR ("Expected line %zu at the end, got %zu\n",
            line0 + nl, xtree_reader_line (reader));
    goto cleanup;
  }
  if (!rdr_expect_get (reader, EOF))
    goto cleanup;
  if (!xtree_reader_eof (reader)) {
    PERROR ("Reader should be at EOF after reading the whole file\n");
    goto cleanup;
  }
  if (!rdr_expect_noerror (reader))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  if (inf)
    fclose (inf);
  if (fname) {
    remove (fname);
    free (fname);
  }
  free (data);
  return ret;
}

// name: test_reader_file_null_input
int test_reader_file_null_input (void)
{
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = NULL;
  char *fname = NULL;
  FILE *inf = NULL;

  // A NULL FILE* must be rejected.
  reader = xtree_reader_file ("test", NULL);
  if (reader != NULL) {
    PERROR ("Creating a reader from a NULL FILE* should return NULL\n");
    goto cleanup;
  }

  // An empty file is valid input and yields immediate EOF, no error.
  if (!rdr_write_tmpfile ("", 0, &fname))
    goto cleanup;
  inf = fopen (fname, "rb");
  if (!inf) {
    PERROR ("Failed to reopen [%s] for reading\n", fname);
    goto cleanup;
  }
  reader = xtree_reader_file ("test", inf);
  if (!reader) {
    PERROR ("Failed to create reader from an empty file\n");
    goto cleanup;
  }
  if (xtree_reader_peek (reader) != EOF) {
    PERROR ("Peek on an empty file should return EOF\n");
    goto cleanup;
  }
  if (!rdr_expect_get (reader, EOF))
    goto cleanup;
  if (!xtree_reader_eof (reader)) {
    PERROR ("Reader should report EOF on an empty file\n");
    goto cleanup;
  }
  if (!rdr_expect_noerror (reader))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  if (inf)
    fclose (inf);
  if (fname) {
    remove (fname);
    free (fname);
  }
  return ret;
}





/* ------------------------------------------------------------------
 * Tests for xtree_json_scan()
 *
 * This scanner is a lenient, JSON-like tokeniser (see xtree_json_scan.h):
 *
 *   - Every call returns a token. EOF (id 0) is returned at the end of the
 *     input, repeatedly if the scanner is called again. ERROR (id
 *     -1) is returned only for a string that is not terminated
 *     before the end of the input. EOF and ERROR have "" as their text.
 *   - Token text is the raw source text. The one exception is inside
 *     strings: a literal LF is returned as the two characters \n and a
 *     literal CR as \r (so CRLF is returned as \r\n). Escape sequences in
 *     strings are never decoded or validated, whatever they are.
 *   - It tokenises only; it does not check the JSON grammar. Trailing
 *     commas, "}{" and "1 2" all scan without complaint.
 *   - Whitespace is space, tab, CR and LF. It is skipped between tokens.
 *   - "//" starts a comment (anywhere outside a string) that runs to the
 *     end of the line. The comment token's text includes the "//" and
 *     excludes the line terminator (LF, and a CR directly before it).
 *   - Anything else is a "word": a run of characters up to whitespace,
 *     one of { } [ ] : , a double quote, or "//". The whole word is
 *     classified: a valid number is NUMBER (sign, decimal with optional
 *     fraction/exponent, ".5" and "5." included, 0x hex, 0b binary,
 *     leading-0 octal); true/false/null/Infinity/+Infinity/-Infinity/NaN/
 *     undefined are their own tokens; everything else is a SYMBOL. So
 *     "0x1abc" is one NUMBER but "1abc", "truex" and "08" are SYMBOLs.
 *
 * Assumptions I had to make (adjust the tables if the library differs):
 *
 *   - Token line/cpos is the position of the token's FIRST character.
 *     As in the reader tests, the absolute origin (0- or 1-based) is not
 *     assumed: positions are expressed relative to the reader's starting
 *     line()/cpos(). The reader resets cpos on '\n', so cpos offsets are
 *     relative to the start of the line even on later lines. '\r' is an
 *     ordinary character that advances cpos. A string that spans lines
 *     keeps the position of its opening quote, and the next token is
 *     positioned after it on the reader's current line.
 *   - The EOF token's position is the reader's position at the end of
 *     the input (after any trailing whitespace has been skipped).
 *   - An ERROR token's line/cpos is the reader's position at the end of
 *     the input (where the missing closing quote was expected).
 *   - After an ERROR token, xtree_reader_error() returns a well-formed
 *     error object; otherwise it returns NULL.
 *   - Whitespace other than the four above (form feed, vertical tab, NBSP)
 *     is not whitespace: it is an ordinary word character.
 *   - Letter case matters: True, NULL, infinity and Undefined are SYMBOLs.
 *     +NaN, -NaN and -undefined are SYMBOLs too.
 *   - A backslash followed directly by a literal LF inside a string is
 *     not tested (the result of rewriting it is ambiguous).
 *   - Nothing is asserted about what the scanner does after an ERROR
 *     token.
 * ------------------------------------------------------------------ */

#define JSN_NAME "test.json"

/* Marks an expected position as "wherever the reader is after the scan
 * call"; used for the EOF token. */
#define JSN_AT_READER ((size_t)-1)

struct jsn_exp {
  int id;
  const char *text;
  size_t line;    /* offset from the reader's starting line() */
  size_t cpos;    /* offset from the reader's starting cpos() */
};

#define JSN_T(id, text, line, cpos)  { (int)(id), (text), (line), (cpos) }
#define JSN_N(a)  (sizeof (a) / sizeof (a)[0])

static const char *jsn_idname (int id)
{
  switch (id) {
    case xtree_json_token_EOF:      return "EOF";
    case xtree_json_token_LBRACE:   return "LBRACE";
    case xtree_json_token_RBRACE:   return "RBRACE";
    case xtree_json_token_LBRACKET: return "LBRACKET";
    case xtree_json_token_RBRACKET: return "RBRACKET";
    case xtree_json_token_COLON:    return "COLON";
    case xtree_json_token_COMMA:    return "COMMA";
    case xtree_json_token_STRING:   return "STRING";
    case xtree_json_token_NUMBER:   return "NUMBER";
    case xtree_json_token_TRUE:     return "TRUE";
    case xtree_json_token_FALSE:    return "FALSE";
    case xtree_json_token_NULL:     return "NULL";
    case xtree_json_token_COMMENT:  return "COMMENT";
    case xtree_json_token_SYMBOL:   return "SYMBOL";
    case xtree_json_token_INFINITY: return "INFINITY";
    case xtree_json_token_NEG_INFINITY: return "NEG_INFINITY";
    case xtree_json_token_NAN:      return "NAN";
    case xtree_json_token_UNDEFINED: return "UNDEFINED";
    case xtree_json_token_ERROR:    return "ERROR";
    default:                        return "(unknown id)";
  }
}

/* Scan one token from the reader and check its id, text, filename,
 * position and the reader's error state. `line` and `cpos` are absolute
 * (JSN_AT_READER for both means "the reader's position after the call").
 * The token is freed. Returns true if everything matched. */
static bool jsn_check_token (xtree_reader_t *reader, int id,
                             const char *text, size_t line, size_t cpos)
{
  bool ok = false;
  xtree_scan_token_t *tok = xtree_json_scan (reader);

  if (!tok) {
    PERROR ("Scanner returned NULL, expected a [%s] token\n",
            jsn_idname (id));
    return false;
  }

  if (line == JSN_AT_READER) {
    line = xtree_reader_line (reader);
    cpos = xtree_reader_cpos (reader);
  }

  int gid = xtree_scan_token_id (tok);
  const char *gtext = xtree_scan_token_text (tok);
  const char *gname = xtree_scan_token_filename (tok);
  size_t gline = xtree_scan_token_line (tok);
  size_t gcpos = xtree_scan_token_cpos (tok);

  if (gid != id) {
    PERROR ("Expected token [%s], got [%s] with text [%.80s]\n",
            jsn_idname (id), jsn_idname (gid), gtext ? gtext : "(null)");
    goto cleanup;
  }
  if (!gtext || strcmp (gtext, text) != 0) {
    PERROR ("[%s]: expected text [%.80s], got [%.80s]\n", jsn_idname (id),
            text, gtext ? gtext : "(null)");
    goto cleanup;
  }
  if (!gname || strcmp (gname, JSN_NAME) != 0) {
    PERROR ("[%s]: expected filename [%s], got [%s]\n", jsn_idname (id),
            JSN_NAME, gname ? gname : "(null)");
    goto cleanup;
  }
  if (gline != line || gcpos != cpos) {
    PERROR ("[%s] text [%.80s]: expected line %zu cpos %zu, got line %zu "
            "cpos %zu\n", jsn_idname (id), text, line, cpos, gline, gcpos);
    goto cleanup;
  }

  if (id == xtree_json_token_ERROR) {
    if (!rdr_expect_wellformed_errobj (xtree_reader_error (reader)))
      goto cleanup;
  } else {
    if (!rdr_expect_noerror (reader))
      goto cleanup;
  }

  ok = true;

cleanup:
  xtree_scan_token_free (&tok);
  return ok;
}

/* Check that the scanner is at the end of the input: it must return EOF
 * (twice, because EOF is sticky) and the reader must report EOF. */
static bool jsn_check_eof (xtree_reader_t *reader)
{
  for (size_t i=0; i<2; i++) {
    if (!jsn_check_token (reader, xtree_json_token_EOF, "",
                          JSN_AT_READER, JSN_AT_READER))
      return false;
  }
  if (!xtree_reader_eof (reader)) {
    PERROR ("Reader should be at EOF after the EOF token\n");
    return false;
  }
  return true;
}

/* Scan `input` and check that it produces exactly the n tokens in `want`
 * followed by EOF. If the last expected token is an ERROR, nothing after
 * it is checked. Positions in `want` are relative to the reader's
 * starting position. */
static bool jsn_run (const char *input, const struct jsn_exp *want, size_t n)
{
  bool ok = false;
  xtree_reader_t *reader = xtree_reader_str (JSN_NAME, input);

  if (!reader) {
    PERROR ("Failed to create reader\n");
    return false;
  }

  size_t line0 = xtree_reader_line (reader);
  size_t cpos0 = xtree_reader_cpos (reader);

  for (size_t i=0; i<n; i++) {
    if (!jsn_check_token (reader, want[i].id, want[i].text,
                          line0 + want[i].line, cpos0 + want[i].cpos)) {
      PERROR ("Token %zu of input [%.80s] was wrong\n", i, input);
      goto cleanup;
    }
  }

  if (!(n && want[n-1].id == xtree_json_token_ERROR)) {
    if (!jsn_check_eof (reader)) {
      PERROR ("Bad end of input [%.80s]\n", input);
      goto cleanup;
    }
  }

  ok = true;

cleanup:
  xtree_reader_free (&reader);
  return ok;
}

/* Input that must produce a single ERROR token at (line, cpos). */
struct jsn_bad_case {
  const char *input;
  size_t line;
  size_t cpos;
};

static int jsn_run_bad_cases (const struct jsn_bad_case *cases, size_t n)
{
  int rc = EXIT_SUCCESS;
  for (size_t i=0; i<n; i++) {
    struct jsn_exp e = JSN_T (xtree_json_token_ERROR, "",
                              cases[i].line, cases[i].cpos);
    if (!jsn_run (cases[i].input, &e, 1))
      rc = EXIT_FAILURE;
  }
  return rc;
}

/* Each input must be scanned as exactly one token of type `id` whose text
 * is the input itself. Each is tried bare and padded with whitespace on
 * both sides (the padding puts the token on the second line). */
static int jsn_run_single_cases (int id, const char *const *inputs,
                                 size_t n)
{
  int rc = EXIT_SUCCESS;
  for (size_t i=0; i<n; i++) {
    struct jsn_exp e = JSN_T (id, inputs[i], 0, 0);
    if (!jsn_run (inputs[i], &e, 1))
      rc = EXIT_FAILURE;

    size_t sz = strlen (inputs[i]) + 16;
    char *padded = malloc (sz);
    if (!padded) {
      PERROR ("Failed to allocate padded input\n");
      rc = EXIT_FAILURE;
      continue;
    }
    snprintf (padded, sz, " \t\r\n%s\n\r\t ", inputs[i]);
    e.line = 1;
    e.cpos = 0;
    if (!jsn_run (padded, &e, 1))
      rc = EXIT_FAILURE;
    free (padded);
  }
  return rc;
}

// name: test_json_scan_eof_empty
int test_json_scan_eof_empty (void)
{
  // No tokens: EOF straight away, at the starting position.
  return jsn_run ("", NULL, 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

// name: test_json_scan_eof_whitespace_only
int test_json_scan_eof_whitespace_only (void)
{
  static const char *const inputs[] = {
    " ", "\t", "\r", "\n", " \t\r\n", "\n\n\n", "  \n\t", "\r\n\r\n",
  };
  int rc = EXIT_SUCCESS;
  for (size_t i=0; i<JSN_N (inputs); i++) {
    if (!jsn_run (inputs[i], NULL, 0))
      rc = EXIT_FAILURE;
  }
  return rc;
}

// name: test_json_scan_punctuation_each
int test_json_scan_punctuation_each (void)
{
  int rc = EXIT_SUCCESS;
  static const struct { int id; const char *text; } p[] = {
    { xtree_json_token_LBRACE,   "{" },
    { xtree_json_token_RBRACE,   "}" },
    { xtree_json_token_LBRACKET, "[" },
    { xtree_json_token_RBRACKET, "]" },
    { xtree_json_token_COLON,    ":" },
    { xtree_json_token_COMMA,    "," },
  };
  for (size_t i=0; i<JSN_N (p); i++) {
    if (jsn_run_single_cases (p[i].id, &p[i].text, 1) != EXIT_SUCCESS)
      rc = EXIT_FAILURE;
  }
  return rc;
}

// name: test_json_scan_punctuation_sequence
int test_json_scan_punctuation_sequence (void)
{
  static const struct jsn_exp want[] = {
    JSN_T (xtree_json_token_LBRACE,   "{", 0, 0),
    JSN_T (xtree_json_token_RBRACE,   "}", 0, 1),
    JSN_T (xtree_json_token_LBRACKET, "[", 0, 2),
    JSN_T (xtree_json_token_RBRACKET, "]", 0, 3),
    JSN_T (xtree_json_token_COLON,    ":", 0, 4),
    JSN_T (xtree_json_token_COMMA,    ",", 0, 5),
  };
  return jsn_run ("{}[]:,", want, JSN_N (want)) ? EXIT_SUCCESS : EXIT_FAILURE;
}

// name: test_json_scan_punctuation_whitespace_separated
int test_json_scan_punctuation_whitespace_separated (void)
{
  static const struct jsn_exp want[] = {
    JSN_T (xtree_json_token_LBRACE,   "{", 0, 1),
    JSN_T (xtree_json_token_RBRACE,   "}", 0, 3),
    JSN_T (xtree_json_token_LBRACKET, "[", 0, 5),
    JSN_T (xtree_json_token_RBRACKET, "]", 0, 7),
    JSN_T (xtree_json_token_COLON,    ":", 0, 9),
    JSN_T (xtree_json_token_COMMA,    ",", 0, 11),
  };
  return jsn_run (" { } [ ] : , ", want, JSN_N (want))
    ? EXIT_SUCCESS : EXIT_FAILURE;
}

// name: test_json_scan_grammar_agnostic
int test_json_scan_grammar_agnostic (void)
{
  // The scanner only tokenises; it must not reject grammatically invalid
  // token streams.
  int rc = EXIT_SUCCESS;

  static const struct jsn_exp backwards[] = {
    JSN_T (xtree_json_token_RBRACE,   "}", 0, 0),
    JSN_T (xtree_json_token_LBRACE,   "{", 0, 1),
    JSN_T (xtree_json_token_RBRACKET, "]", 0, 2),
    JSN_T (xtree_json_token_LBRACKET, "[", 0, 3),
    JSN_T (xtree_json_token_COMMA,    ",", 0, 4),
    JSN_T (xtree_json_token_COLON,    ":", 0, 5),
  };
  if (!jsn_run ("}{][,:", backwards, JSN_N (backwards)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp mixed[] = {
    JSN_T (xtree_json_token_NUMBER, "1",   0, 0),
    JSN_T (xtree_json_token_STRING, "\"a\"", 0, 2),
    JSN_T (xtree_json_token_TRUE,   "true", 0, 6),
    JSN_T (xtree_json_token_NULL,   "null", 0, 11),
  };
  if (!jsn_run ("1 \"a\" true null", mixed, JSN_N (mixed)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp double_comma[] = {
    JSN_T (xtree_json_token_LBRACKET, "[", 0, 0),
    JSN_T (xtree_json_token_NUMBER,   "1", 0, 1),
    JSN_T (xtree_json_token_COMMA,    ",", 0, 2),
    JSN_T (xtree_json_token_COMMA,    ",", 0, 3),
    JSN_T (xtree_json_token_RBRACKET, "]", 0, 4),
  };
  if (!jsn_run ("[1,,]", double_comma, JSN_N (double_comma)))
    rc = EXIT_FAILURE;

  return rc;
}

// name: test_json_scan_literals_each
int test_json_scan_literals_each (void)
{
  static const char *const t[] = { "true" };
  static const char *const f[] = { "false" };
  static const char *const n[] = { "null" };
  int rc = EXIT_SUCCESS;

  if (jsn_run_single_cases (xtree_json_token_TRUE, t, 1) != EXIT_SUCCESS)
    rc = EXIT_FAILURE;
  if (jsn_run_single_cases (xtree_json_token_FALSE, f, 1) != EXIT_SUCCESS)
    rc = EXIT_FAILURE;
  if (jsn_run_single_cases (xtree_json_token_NULL, n, 1) != EXIT_SUCCESS)
    rc = EXIT_FAILURE;
  return rc;
}

// name: test_json_scan_literals_sequence
int test_json_scan_literals_sequence (void)
{
  static const struct jsn_exp want[] = {
    JSN_T (xtree_json_token_TRUE,  "true",  0, 0),
    JSN_T (xtree_json_token_FALSE, "false", 0, 5),
    JSN_T (xtree_json_token_NULL,  "null",  0, 11),
  };
  return jsn_run ("true false null", want, JSN_N (want))
    ? EXIT_SUCCESS : EXIT_FAILURE;
}

// name: test_json_scan_literals_in_structure
int test_json_scan_literals_in_structure (void)
{
  // Literals directly followed by punctuation, with no whitespace.
  int rc = EXIT_SUCCESS;

  static const struct jsn_exp arr[] = {
    JSN_T (xtree_json_token_LBRACKET, "[",     0, 0),
    JSN_T (xtree_json_token_TRUE,     "true",  0, 1),
    JSN_T (xtree_json_token_COMMA,    ",",     0, 5),
    JSN_T (xtree_json_token_FALSE,    "false", 0, 6),
    JSN_T (xtree_json_token_COMMA,    ",",     0, 11),
    JSN_T (xtree_json_token_NULL,     "null",  0, 12),
    JSN_T (xtree_json_token_RBRACKET, "]",     0, 16),
  };
  if (!jsn_run ("[true,false,null]", arr, JSN_N (arr)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp obj[] = {
    JSN_T (xtree_json_token_LBRACE, "{",     0, 0),
    JSN_T (xtree_json_token_STRING, "\"k\"", 0, 1),
    JSN_T (xtree_json_token_COLON,  ":",     0, 4),
    JSN_T (xtree_json_token_TRUE,   "true",  0, 5),
    JSN_T (xtree_json_token_RBRACE, "}",     0, 9),
  };
  if (!jsn_run ("{\"k\":true}", obj, JSN_N (obj)))
    rc = EXIT_FAILURE;

  return rc;
}

// name: test_json_scan_strings_valid
int test_json_scan_strings_valid (void)
{
  // Each string is returned fully quoted with every escape left exactly
  // as written. The C literals below are the raw JSON text.
  static const char *const cases[] = {
    "\"\"",                                   /* empty */
    "\" \"",                                  /* just a space */
    "\"abc\"",
    "\"hello world\"",
    "\"\\\"\"",                               /* "\""   */
    "\"\\\\\"",                               /* "\\"   */
    "\"a\\\\\"",                              /* "a\\"  escaped backslash
                                                 right before the quote */
    "\"\\\\n\"",                              /* "\\n"  backslash, then n */
    "\"\\/\"",                                /* "\/"   */
    "\"\\b\\f\\n\\r\\t\"",                    /* all the short escapes */
    "\"\\u0041\"",
    "\"\\u00e9\\u00E9\\uabcd\\uABCD\\u0000\"", /* hex case, \u0000 */
    "\"\\uD83D\\uDE00\"",                     /* surrogate pair, raw */
    "\"/\"",                                  /* unescaped solidus */
    "\"'\"",                                  /* single quote */
    "\"{}[]:,\"",                             /* structural characters */
    "\"true false null 123\"",
    "\"// not a comment\"",
    "\"a//b\"",
    "\"\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80\"", /* UTF-8, raw bytes */
    "\"\x7f\"",                               /* DEL */
    "\"a\tb\"",                               /* literal tab stays literal */
    "\"\x01\"",                               /* so do other controls */
  };
  return jsn_run_single_cases (xtree_json_token_STRING, cases, JSN_N (cases));
}

// name: test_json_scan_strings_sequence
int test_json_scan_strings_sequence (void)
{
  int rc = EXIT_SUCCESS;

  // A key/value pair, and two strings with nothing between them (the
  // scanner does not require separators).
  static const struct jsn_exp pair[] = {
    JSN_T (xtree_json_token_STRING, "\"a\"", 0, 0),
    JSN_T (xtree_json_token_COLON,  ":",     0, 3),
    JSN_T (xtree_json_token_STRING, "\"b\"", 0, 4),
  };
  if (!jsn_run ("\"a\":\"b\"", pair, JSN_N (pair)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp adjacent[] = {
    JSN_T (xtree_json_token_STRING, "\"a\"", 0, 0),
    JSN_T (xtree_json_token_STRING, "\"b\"", 0, 3),
  };
  if (!jsn_run ("\"a\"\"b\"", adjacent, JSN_N (adjacent)))
    rc = EXIT_FAILURE;

  // "a\\" ends at the second quote, because the backslash before it is
  // itself escaped; the comma and number after it are separate tokens.
  static const struct jsn_exp escaped_backslash[] = {
    JSN_T (xtree_json_token_STRING, "\"a\\\\\"", 0, 0),
    JSN_T (xtree_json_token_COMMA,  ",",         0, 5),
    JSN_T (xtree_json_token_NUMBER, "1",         0, 6),
  };
  if (!jsn_run ("\"a\\\\\",1", escaped_backslash, JSN_N (escaped_backslash)))
    rc = EXIT_FAILURE;

  // An escaped quote does not end the string.
  static const struct jsn_exp escaped_quote[] = {
    JSN_T (xtree_json_token_STRING, "\"a\\\",b\"", 0, 0),
    JSN_T (xtree_json_token_COMMA,  ",",           0, 7),
  };
  if (!jsn_run ("\"a\\\",b\",", escaped_quote, JSN_N (escaped_quote)))
    rc = EXIT_FAILURE;

  return rc;
}

// name: test_json_scan_numbers_valid
int test_json_scan_numbers_valid (void)
{
  // Returned as raw text; nothing is normalised ("1.0" stays "1.0",
  // "1E10" stays "1E10", "-0" stays "-0", "0x1F" stays "0x1F").
  static const char *const cases[] = {
    /* decimal integers */
    "0", "-0", "+0", "1", "-1", "+1", "9", "10", "123", "1234567890",
    "-1234567890", "+1234567890",
    "12345678901234567890123456789012345678901234567890",
    /* fractions, including a missing integer or fraction part */
    "0.5", "-0.5", "+0.5", ".5", "-.5", "+.5", "0.", "5.", "-5.",
    "0.0", "-0.0", "1.0", "10.25", "0.1", "123.456",
    /* exponents */
    "1e10", "1E10", "1e+10", "1e-10", "1E+10", "1E-2", "1e0", "0e0",
    "-0e-0", "0.0e+0", "0e1", "+1e1", "-1.5e+3", ".5e3", "1.e5",
    "123.456E789", "1.7976931348623157e308", "5e-324", "1e999999",
    /* hexadecimal */
    "0x0", "0x1", "0xF", "0xff", "0XFF", "0xdeadBEEF", "0x1abc",
    "-0x10", "+0x10", "0x0123456789abcdefABCDEF",
    /* octal (a leading zero followed by octal digits) */
    "00", "01", "07", "0777", "-017", "+07",
    /* binary */
    "0b0", "0b1", "0b1010", "0B11", "-0b1", "+0b1", "0b00001111",
  };
  return jsn_run_single_cases (xtree_json_token_NUMBER, cases, JSN_N (cases));
}

// name: test_json_scan_numbers_delimited
int test_json_scan_numbers_delimited (void)
{
  // A number ends at the first character that cannot continue it.
  int rc = EXIT_SUCCESS;

  static const struct jsn_exp comma[] = {
    JSN_T (xtree_json_token_NUMBER, "1", 0, 0),
    JSN_T (xtree_json_token_COMMA,  ",", 0, 1),
    JSN_T (xtree_json_token_NUMBER, "2", 0, 2),
  };
  if (!jsn_run ("1,2", comma, JSN_N (comma)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp list[] = {
    JSN_T (xtree_json_token_LBRACKET, "[",   0, 0),
    JSN_T (xtree_json_token_NUMBER,   "0",   0, 1),
    JSN_T (xtree_json_token_COMMA,    ",",   0, 2),
    JSN_T (xtree_json_token_NUMBER,   "-0",  0, 3),
    JSN_T (xtree_json_token_COMMA,    ",",   0, 5),
    JSN_T (xtree_json_token_NUMBER,   "0.5", 0, 6),
    JSN_T (xtree_json_token_RBRACKET, "]",   0, 9),
  };
  if (!jsn_run ("[0,-0,0.5]", list, JSN_N (list)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp object[] = {
    JSN_T (xtree_json_token_LBRACE, "{",        0, 0),
    JSN_T (xtree_json_token_STRING, "\"a\"",    0, 1),
    JSN_T (xtree_json_token_COLON,  ":",        0, 4),
    JSN_T (xtree_json_token_NUMBER, "-1.5e+3",  0, 5),
    JSN_T (xtree_json_token_RBRACE, "}",        0, 12),
  };
  if (!jsn_run ("{\"a\":-1.5e+3}", object, JSN_N (object)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp spaced[] = {
    JSN_T (xtree_json_token_NUMBER, "1", 0, 0),
    JSN_T (xtree_json_token_NUMBER, "2", 0, 2),
    JSN_T (xtree_json_token_NUMBER, "3", 0, 4),
  };
  if (!jsn_run ("1 2 3", spaced, JSN_N (spaced)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp newline[] = {
    JSN_T (xtree_json_token_NUMBER, "1", 0, 0),
    JSN_T (xtree_json_token_NUMBER, "2", 1, 0),
  };
  if (!jsn_run ("1\n2", newline, JSN_N (newline)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp colon[] = {
    JSN_T (xtree_json_token_NUMBER, "1", 0, 0),
    JSN_T (xtree_json_token_COLON,  ":", 0, 1),
    JSN_T (xtree_json_token_NUMBER, "2", 0, 2),
  };
  if (!jsn_run ("1:2", colon, JSN_N (colon)))
    rc = EXIT_FAILURE;

  // The word is classified as a whole: "0x1 abc" is a number and a symbol,
  // while "0x1abc" (see test_json_scan_numbers_valid) is a single number.
  static const struct jsn_exp hex_then_symbol[] = {
    JSN_T (xtree_json_token_NUMBER, "0x1", 0, 0),
    JSN_T (xtree_json_token_SYMBOL, "abc", 0, 4),
  };
  if (!jsn_run ("0x1 abc", hex_then_symbol, JSN_N (hex_then_symbol)))
    rc = EXIT_FAILURE;

  return rc;
}

// name: test_json_scan_whitespace_kinds
int test_json_scan_whitespace_kinds (void)
{
  int rc = EXIT_SUCCESS;

  // Tabs.
  static const struct jsn_exp tabs[] = {
    JSN_T (xtree_json_token_LBRACKET, "[", 0, 0),
    JSN_T (xtree_json_token_NUMBER,   "1", 0, 2),
    JSN_T (xtree_json_token_COMMA,    ",", 0, 3),
    JSN_T (xtree_json_token_NUMBER,   "2", 0, 5),
    JSN_T (xtree_json_token_RBRACKET, "]", 0, 7),
  };
  if (!jsn_run ("[\t1,\t2\t]", tabs, JSN_N (tabs)))
    rc = EXIT_FAILURE;

  // CRLF line endings: only '\n' starts a new line; '\r' is skipped like
  // any other whitespace.
  static const struct jsn_exp crlf[] = {
    JSN_T (xtree_json_token_LBRACE, "{",     0, 0),
    JSN_T (xtree_json_token_STRING, "\"a\"", 1, 2),
    JSN_T (xtree_json_token_COLON,  ":",     1, 6),
    JSN_T (xtree_json_token_NUMBER, "1",     2, 1),
    JSN_T (xtree_json_token_RBRACE, "}",     3, 0),
  };
  if (!jsn_run ("{\r\n  \"a\" :\r\n 1\r\n}\r\n", crlf, JSN_N (crlf)))
    rc = EXIT_FAILURE;

  // A mix of everything, with blank lines and leading/trailing space.
  static const struct jsn_exp mixed[] = {
    JSN_T (xtree_json_token_LBRACKET, "[", 1, 1),
    JSN_T (xtree_json_token_NUMBER,   "1", 3, 2),
  };
  if (!jsn_run (" \t\r\n [ \n\n  1 ", mixed, JSN_N (mixed)))
    rc = EXIT_FAILURE;

  return rc;
}

// name: test_json_scan_full_document
int test_json_scan_full_document (void)
{
  static const char *const input =
    "{\n"
    "  \"name\": \"x\",\n"
    "  \"list\": [1, 2.5, -3e2, true, false, null],\n"
    "  \"obj\": {\"k\": \"v\\n\"}\n"
    "}\n";

  static const struct jsn_exp want[] = {
    JSN_T (xtree_json_token_LBRACE,   "{",        0, 0),
    JSN_T (xtree_json_token_STRING,   "\"name\"", 1, 2),
    JSN_T (xtree_json_token_COLON,    ":",        1, 8),
    JSN_T (xtree_json_token_STRING,   "\"x\"",    1, 10),
    JSN_T (xtree_json_token_COMMA,    ",",        1, 13),
    JSN_T (xtree_json_token_STRING,   "\"list\"", 2, 2),
    JSN_T (xtree_json_token_COLON,    ":",        2, 8),
    JSN_T (xtree_json_token_LBRACKET, "[",        2, 10),
    JSN_T (xtree_json_token_NUMBER,   "1",        2, 11),
    JSN_T (xtree_json_token_COMMA,    ",",        2, 12),
    JSN_T (xtree_json_token_NUMBER,   "2.5",      2, 14),
    JSN_T (xtree_json_token_COMMA,    ",",        2, 17),
    JSN_T (xtree_json_token_NUMBER,   "-3e2",     2, 19),
    JSN_T (xtree_json_token_COMMA,    ",",        2, 23),
    JSN_T (xtree_json_token_TRUE,     "true",     2, 25),
    JSN_T (xtree_json_token_COMMA,    ",",        2, 29),
    JSN_T (xtree_json_token_FALSE,    "false",    2, 31),
    JSN_T (xtree_json_token_COMMA,    ",",        2, 36),
    JSN_T (xtree_json_token_NULL,     "null",     2, 38),
    JSN_T (xtree_json_token_RBRACKET, "]",        2, 42),
    JSN_T (xtree_json_token_COMMA,    ",",        2, 43),
    JSN_T (xtree_json_token_STRING,   "\"obj\"",  3, 2),
    JSN_T (xtree_json_token_COLON,    ":",        3, 7),
    JSN_T (xtree_json_token_LBRACE,   "{",        3, 9),
    JSN_T (xtree_json_token_STRING,   "\"k\"",    3, 10),
    JSN_T (xtree_json_token_COLON,    ":",        3, 13),
    JSN_T (xtree_json_token_STRING,   "\"v\\n\"", 3, 15),
    JSN_T (xtree_json_token_RBRACE,   "}",        3, 20),
    JSN_T (xtree_json_token_RBRACE,   "}",        4, 0),
  };
  return jsn_run (input, want, JSN_N (want)) ? EXIT_SUCCESS : EXIT_FAILURE;
}

// name: test_json_scan_token_filename
int test_json_scan_token_filename (void)
{
  // The filename on each token is the name the reader was created with.
  // (jsn_check_token() checks JSN_NAME; this checks a different name, so a
  // hard-coded filename in the scanner would be caught.)
  int ret = EXIT_FAILURE;
  static const char *const name = "dir/other-name.json";
  xtree_reader_t *reader = xtree_reader_str (name, "[1]");
  xtree_scan_token_t *tok = NULL;

  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  for (size_t i=0; i<4; i++) {     /* '[', '1', ']', EOF */
    tok = xtree_json_scan (reader);
    if (!tok) {
      PERROR ("Scanner returned NULL for token %zu\n", i);
      goto cleanup;
    }
    const char *got = xtree_scan_token_filename (tok);
    if (!got || strcmp (got, name) != 0) {
      PERROR ("Token %zu: expected filename [%s], got [%s]\n",
              i, name, got ? got : "(null)");
      goto cleanup;
    }
    xtree_scan_token_free (&tok);
    tok = NULL;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_scan_token_free (&tok);
  xtree_reader_free (&reader);
  return ret;
}

// name: test_json_scan_string_escapes_not_decoded
int test_json_scan_string_escapes_not_decoded (void)
{
  // The scanner neither decodes nor validates escapes: whatever follows a
  // backslash is kept as written, including sequences that are not valid
  // JSON. Each text equals its input.
  static const char *const cases[] = {
    "\"\\q\"",
    "\"\\x41\"",
    "\"\\u\"",                    /* \u with no digits */
    "\"\\u12\"",                  /* too few digits */
    "\"\\u12G4\"",                /* non-hex digit */
    "\"\\uXYZW\"",
    "\"\\'\"",
    "\"\\a\"",
    "\"\\0\"",
    "\"\\U0041\"",
    "\"\\e\"",
    "\"\\ \"",                    /* backslash space */
    "\"\\\\q\"",                  /* escaped backslash, then q */
    "\"\\n\"",                    /* the two characters \ and n */
    "\"\\r\\n\"",
    "\"C:\\\\dir\\file\"",
    "\"\\u0041\\\\u0041\"",
  };
  return jsn_run_single_cases (xtree_json_token_STRING, cases, JSN_N (cases));
}

// name: test_json_scan_strings_multiline
int test_json_scan_strings_multiline (void)
{
  // A string may contain literal newlines. In the returned text a literal
  // LF becomes \n and a literal CR becomes \r, so CRLF becomes \r\n.
  // Tabs, other characters and escapes already present are untouched.
  int rc = EXIT_SUCCESS;

  static const struct { const char *input; const char *text; } cases[] = {
    { "\"a\nb\"",       "\"a\\nb\"" },
    { "\"\n\"",         "\"\\n\"" },
    { "\"\n\n\n\"",     "\"\\n\\n\\n\"" },
    { "\"a\r\nb\"",     "\"a\\r\\nb\"" },
    { "\"a\rb\"",       "\"a\\rb\"" },
    { "\"\r\n\r\n\"",   "\"\\r\\n\\r\\n\"" },
    { "\"a\tb\"",       "\"a\tb\"" },
    { "\"a\n\tb\"",     "\"a\\n\tb\"" },
    { "\"a\\nb\"",      "\"a\\nb\"" },         /* already escaped: as-is */
    { "\"x\\n\ny\"",    "\"x\\n\\ny\"" },      /* escaped, then literal */
    { "\"a\\\"\nb\"",   "\"a\\\"\\nb\"" },     /* escaped quote, then LF */
    { "\"a\n// not\"",  "\"a\\n// not\"" },    /* not a comment */
    { "\"{\n}\"",       "\"{\\n}\"" },
    { "\"line one\nline two\nline three\"",
      "\"line one\\nline two\\nline three\"" },
  };
  for (size_t i=0; i<JSN_N (cases); i++) {
    struct jsn_exp e = JSN_T (xtree_json_token_STRING, cases[i].text, 0, 0);
    if (!jsn_run (cases[i].input, &e, 1))
      rc = EXIT_FAILURE;
  }

  // Positions: the string keeps the position of its opening quote, and
  // the tokens after it are positioned on the line the reader is now on.
  static const struct jsn_exp then_number[] = {
    JSN_T (xtree_json_token_STRING, "\"a\\nb\"", 0, 0),
    JSN_T (xtree_json_token_NUMBER, "1",         1, 3),
  };
  if (!jsn_run ("\"a\nb\" 1", then_number, JSN_N (then_number)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp in_array[] = {
    JSN_T (xtree_json_token_LBRACKET, "[",             0, 0),
    JSN_T (xtree_json_token_STRING,   "\"a\\r\\nb\"",  0, 1),
    JSN_T (xtree_json_token_COMMA,    ",",             1, 2),
    JSN_T (xtree_json_token_NUMBER,   "1",             2, 2),
    JSN_T (xtree_json_token_RBRACKET, "]",             2, 3),
  };
  if (!jsn_run ("[\"a\r\nb\",\n  1]", in_array, JSN_N (in_array)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp then_comment[] = {
    JSN_T (xtree_json_token_STRING,  "\"a\\n\"", 0, 0),
    JSN_T (xtree_json_token_COMMENT, "// c",     1, 2),
  };
  if (!jsn_run ("\"a\n\" // c", then_comment, JSN_N (then_comment)))
    rc = EXIT_FAILURE;

  return rc;
}

// name: test_json_scan_symbols_single
int test_json_scan_symbols_single (void)
{
  // Each input is one SYMBOL whose text is the input itself. Anything that
  // is not whitespace, punctuation, a string, a comment, a number or one
  // of the keyword tokens is a symbol, as is any word that merely starts
  // like one of those.
  static const char *const cases[] = {
    /* ordinary identifiers */
    "x", "SomeVar", "some_var", "_x", "x1", "ONE", "TWO)", "some-name",
    "a.b.c", "$", "$(ONE)",
    /* punctuation and operators that are not JSON */
    "@", "#", "'x'", "<", "=", ";", "(", "\\", "*",
    "/", "/x", "a/b", "/*", "*/",
    /* keywords with the wrong case, or cut short, or extended */
    "True", "TRUE", "False", "FALSE", "Null", "NULL",
    "t", "tr", "tru", "f", "fals", "n", "nul", "nil",
    "trux", "trUe", "fAlse", "nulL",
    "truex", "falsey", "nullable", "trueish", "nullx",
    "Undefined", "UNDEFINED", "undefinedx",
    "infinity", "INFINITY", "Infinityx", "+Infinit", "-Infinity2",
    "--Infinity", "++Infinity", "Infinity-",
    "nan", "NAN", "NaNa", "+NaN", "-NaN", "-undefined", "+undefined",
    /* things that look like numbers but are not numbers as a whole */
    "1abc", "12px", "1e", "1E", "1e+", "1e-", "1e+x", "1.5e", "1.5.5",
    "1.2.3", "1e5e5", "0.5.", "5..", "1_000", "2026-10-25", "1-2", "1+1",
    "08", "09", "007e", "00.5", "01.5", "012e3", "0o17",
    "0x", "0X", "0xg", "0x1g", "0x-1", "-0x", "0b", "0B", "0b2", "0b12",
    "+0b",
    /* signs and dots without digits */
    "-", "+", "-a", "--1", "-+1", "+-1", ".", "..", "..5", ".e5", "-.",
    "+.",
    /* control characters and non-ASCII bytes are ordinary word characters
     * (and form feed, vertical tab and NBSP are not whitespace) */
    "\x01", "\x7f", "\f", "\v", "\xc2\xa0", "\xc3\xa9", "\xff",
    "caf\xc3\xa9", "\xf0\x9f\x98\x80",
  };
  return jsn_run_single_cases (xtree_json_token_SYMBOL, cases, JSN_N (cases));
}

// name: test_json_scan_symbols_delimited
int test_json_scan_symbols_delimited (void)
{
  // A word ends at whitespace, at { } [ ] : , at a double quote, and at
  // "//". Symbols therefore never swallow JSON structure.
  int rc = EXIT_SUCCESS;

  static const struct jsn_exp parens[] = {
    JSN_T (xtree_json_token_SYMBOL, "$(ONE", 0, 0),
    JSN_T (xtree_json_token_SYMBOL, "TWO)",  0, 6),
  };
  if (!jsn_run ("$(ONE TWO)", parens, JSN_N (parens)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp brace[] = {
    JSN_T (xtree_json_token_SYMBOL, "ONE", 0, 0),
    JSN_T (xtree_json_token_LBRACE, "{",   0, 3),
    JSN_T (xtree_json_token_SYMBOL, "TWO", 0, 4),
  };
  if (!jsn_run ("ONE{TWO", brace, JSN_N (brace)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp bracket[] = {
    JSN_T (xtree_json_token_SYMBOL,   "a", 0, 0),
    JSN_T (xtree_json_token_RBRACKET, "]", 0, 1),
    JSN_T (xtree_json_token_SYMBOL,   "b", 0, 2),
  };
  if (!jsn_run ("a]b", bracket, JSN_N (bracket)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp unquoted_key[] = {
    JSN_T (xtree_json_token_LBRACE, "{",   0, 0),
    JSN_T (xtree_json_token_SYMBOL, "foo", 0, 1),
    JSN_T (xtree_json_token_COLON,  ":",   0, 4),
    JSN_T (xtree_json_token_NUMBER, "1",   0, 5),
    JSN_T (xtree_json_token_RBRACE, "}",   0, 6),
  };
  if (!jsn_run ("{foo:1}", unquoted_key, JSN_N (unquoted_key)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp list[] = {
    JSN_T (xtree_json_token_LBRACKET, "[", 0, 0),
    JSN_T (xtree_json_token_SYMBOL,   "A", 0, 1),
    JSN_T (xtree_json_token_COMMA,    ",", 0, 2),
    JSN_T (xtree_json_token_SYMBOL,   "B", 0, 3),
    JSN_T (xtree_json_token_RBRACKET, "]", 0, 4),
  };
  if (!jsn_run ("[A,B]", list, JSN_N (list)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp spaced[] = {
    JSN_T (xtree_json_token_LBRACE,   "{",       0, 0),
    JSN_T (xtree_json_token_SYMBOL,   "SomeVar", 0, 2),
    JSN_T (xtree_json_token_COLON,    ":",       0, 10),
    JSN_T (xtree_json_token_LBRACKET, "[",       0, 12),
    JSN_T (xtree_json_token_SYMBOL,   "a",       0, 14),
    JSN_T (xtree_json_token_COMMA,    ",",       0, 16),
    JSN_T (xtree_json_token_SYMBOL,   "b",       0, 18),
    JSN_T (xtree_json_token_RBRACKET, "]",       0, 20),
    JSN_T (xtree_json_token_RBRACE,   "}",       0, 22),
  };
  if (!jsn_run ("{ SomeVar : [ a , b ] }", spaced, JSN_N (spaced)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp key_value[] = {
    JSN_T (xtree_json_token_SYMBOL, "key",   0, 0),
    JSN_T (xtree_json_token_COLON,  ":",     0, 3),
    JSN_T (xtree_json_token_SYMBOL, "value", 0, 4),
  };
  if (!jsn_run ("key:value", key_value, JSN_N (key_value)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp comma[] = {
    JSN_T (xtree_json_token_SYMBOL, "SomeVar", 0, 0),
    JSN_T (xtree_json_token_COMMA,  ",",       0, 7),
    JSN_T (xtree_json_token_SYMBOL, "Other",   0, 8),
  };
  if (!jsn_run ("SomeVar,Other", comma, JSN_N (comma)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp quote[] = {
    JSN_T (xtree_json_token_SYMBOL, "a",     0, 0),
    JSN_T (xtree_json_token_STRING, "\"b\"", 0, 1),
  };
  if (!jsn_run ("a\"b\"", quote, JSN_N (quote)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp comment[] = {
    JSN_T (xtree_json_token_SYMBOL,  "a",   0, 0),
    JSN_T (xtree_json_token_COMMENT, "//b", 0, 1),
  };
  if (!jsn_run ("a//b", comment, JSN_N (comment)))
    rc = EXIT_FAILURE;

  // A colon ends a word, so an unquoted URL is symbol, colon, comment.
  static const struct jsn_exp url[] = {
    JSN_T (xtree_json_token_SYMBOL,  "http",     0, 0),
    JSN_T (xtree_json_token_COLON,   ":",        0, 4),
    JSN_T (xtree_json_token_COMMENT, "//x.com",  0, 5),
  };
  if (!jsn_run ("http://x.com", url, JSN_N (url)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp lines[] = {
    JSN_T (xtree_json_token_SYMBOL, "x", 0, 0),
    JSN_T (xtree_json_token_SYMBOL, "y", 1, 0),
  };
  if (!jsn_run ("x\ny", lines, JSN_N (lines)))
    rc = EXIT_FAILURE;

  return rc;
}

// name: test_json_scan_special_values
int test_json_scan_special_values (void)
{
  int rc = EXIT_SUCCESS;

  static const char *const pos_inf[] = { "Infinity", "+Infinity" };
  static const char *const neg_inf[] = { "-Infinity" };
  static const char *const nan[]     = { "NaN" };
  static const char *const undef[]   = { "undefined" };

  if (jsn_run_single_cases (xtree_json_token_INFINITY, pos_inf,
                            JSN_N (pos_inf)) != EXIT_SUCCESS)
    rc = EXIT_FAILURE;
  if (jsn_run_single_cases (xtree_json_token_NEG_INFINITY, neg_inf,
                            JSN_N (neg_inf)) != EXIT_SUCCESS)
    rc = EXIT_FAILURE;
  if (jsn_run_single_cases (xtree_json_token_NAN, nan,
                            JSN_N (nan)) != EXIT_SUCCESS)
    rc = EXIT_FAILURE;
  if (jsn_run_single_cases (xtree_json_token_UNDEFINED, undef,
                            JSN_N (undef)) != EXIT_SUCCESS)
    rc = EXIT_FAILURE;

  static const struct jsn_exp seq[] = {
    JSN_T (xtree_json_token_INFINITY,     "Infinity",  0, 0),
    JSN_T (xtree_json_token_NEG_INFINITY, "-Infinity", 0, 9),
    JSN_T (xtree_json_token_INFINITY,     "+Infinity", 0, 19),
    JSN_T (xtree_json_token_NAN,          "NaN",       0, 29),
    JSN_T (xtree_json_token_UNDEFINED,    "undefined", 0, 33),
  };
  if (!jsn_run ("Infinity -Infinity +Infinity NaN undefined",
                seq, JSN_N (seq)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp in_array[] = {
    JSN_T (xtree_json_token_LBRACKET,     "[",         0, 0),
    JSN_T (xtree_json_token_INFINITY,     "Infinity",  0, 1),
    JSN_T (xtree_json_token_COMMA,        ",",         0, 9),
    JSN_T (xtree_json_token_NEG_INFINITY, "-Infinity", 0, 10),
    JSN_T (xtree_json_token_COMMA,        ",",         0, 19),
    JSN_T (xtree_json_token_NAN,          "NaN",       0, 20),
    JSN_T (xtree_json_token_RBRACKET,     "]",         0, 23),
  };
  if (!jsn_run ("[Infinity,-Infinity,NaN]", in_array, JSN_N (in_array)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp in_object[] = {
    JSN_T (xtree_json_token_LBRACE,    "{",         0, 0),
    JSN_T (xtree_json_token_STRING,    "\"a\"",     0, 1),
    JSN_T (xtree_json_token_COLON,     ":",         0, 4),
    JSN_T (xtree_json_token_UNDEFINED, "undefined", 0, 5),
    JSN_T (xtree_json_token_RBRACE,    "}",         0, 14),
  };
  if (!jsn_run ("{\"a\":undefined}", in_object, JSN_N (in_object)))
    rc = EXIT_FAILURE;

  return rc;
}

// name: test_json_scan_comments_single
int test_json_scan_comments_single (void)
{
  // Each input is one COMMENT whose text is the input itself: everything
  // after "//" is comment, whatever it contains.
  static const char *const cases[] = {
    "//", "// hello", "//x", "///", "///x", "// // nested", "//a//b",
    "// \"quoted\"", "// \"unterminated string",
    "// {}[]:,", "// a/*b*/", "//\ttab", "// \xc3\xa9", "// 1 2 3 true",
    "// http://x.com", "// 'x", "// \\", "// \\n",
  };
  return jsn_run_single_cases (xtree_json_token_COMMENT, cases,
                               JSN_N (cases));
}

// name: test_json_scan_comments_in_context
int test_json_scan_comments_in_context (void)
{
  int rc = EXIT_SUCCESS;

  // A comment ends at LF; the LF is not part of it.
  static const struct jsn_exp lf[] = {
    JSN_T (xtree_json_token_COMMENT, "// hi", 0, 0),
  };
  if (!jsn_run ("// hi\n", lf, JSN_N (lf)))
    rc = EXIT_FAILURE;

  // CRLF: neither the CR nor the LF is part of the comment.
  static const struct jsn_exp crlf[] = {
    JSN_T (xtree_json_token_COMMENT, "// hi", 0, 0),
    JSN_T (xtree_json_token_NUMBER,  "1",     1, 0),
  };
  if (!jsn_run ("// hi\r\n1", crlf, JSN_N (crlf)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp trailing[] = {
    JSN_T (xtree_json_token_NUMBER,  "1",      0, 0),
    JSN_T (xtree_json_token_COMMENT, "// one", 0, 2),
    JSN_T (xtree_json_token_NUMBER,  "2",      1, 0),
    JSN_T (xtree_json_token_COMMENT, "// two", 1, 2),
  };
  if (!jsn_run ("1 // one\n2 // two\n", trailing, JSN_N (trailing)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp in_array[] = {
    JSN_T (xtree_json_token_LBRACKET, "[",          0, 0),
    JSN_T (xtree_json_token_NUMBER,   "1",          0, 1),
    JSN_T (xtree_json_token_COMMA,    ",",          0, 2),
    JSN_T (xtree_json_token_COMMENT,  "// first",   0, 4),
    JSN_T (xtree_json_token_NUMBER,   "2",          1, 1),
    JSN_T (xtree_json_token_RBRACKET, "]",          1, 2),
  };
  if (!jsn_run ("[1, // first\n 2]", in_array, JSN_N (in_array)))
    rc = EXIT_FAILURE;

  // The comment starts right after the previous token, with no space.
  static const struct jsn_exp after_number[] = {
    JSN_T (xtree_json_token_NUMBER,  "1",   0, 0),
    JSN_T (xtree_json_token_COMMENT, "//c", 0, 1),
  };
  if (!jsn_run ("1//c", after_number, JSN_N (after_number)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp after_literal[] = {
    JSN_T (xtree_json_token_TRUE,    "true", 0, 0),
    JSN_T (xtree_json_token_COMMENT, "//c",  0, 4),
  };
  if (!jsn_run ("true//c", after_literal, JSN_N (after_literal)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp after_special[] = {
    JSN_T (xtree_json_token_INFINITY, "Infinity", 0, 0),
    JSN_T (xtree_json_token_COMMENT,  "//c",      0, 8),
  };
  if (!jsn_run ("Infinity//c", after_special, JSN_N (after_special)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp after_string[] = {
    JSN_T (xtree_json_token_STRING,  "\"s\"", 0, 0),
    JSN_T (xtree_json_token_COMMENT, "//c",   0, 3),
  };
  if (!jsn_run ("\"s\"//c", after_string, JSN_N (after_string)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp after_brace[] = {
    JSN_T (xtree_json_token_LBRACE,  "{",   0, 0),
    JSN_T (xtree_json_token_COMMENT, "//c", 0, 1),
    JSN_T (xtree_json_token_RBRACE,  "}",   1, 0),
  };
  if (!jsn_run ("{//c\n}", after_brace, JSN_N (after_brace)))
    rc = EXIT_FAILURE;

  // Consecutive comments are separate tokens, and blank lines are skipped.
  static const struct jsn_exp consecutive[] = {
    JSN_T (xtree_json_token_COMMENT, "//a", 0, 0),
    JSN_T (xtree_json_token_COMMENT, "//b", 1, 0),
  };
  if (!jsn_run ("//a\n//b", consecutive, JSN_N (consecutive)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp blank_lines[] = {
    JSN_T (xtree_json_token_COMMENT, "// a", 0, 0),
    JSN_T (xtree_json_token_NUMBER,  "1",    3, 0),
  };
  if (!jsn_run ("// a\n\n\n1", blank_lines, JSN_N (blank_lines)))
    rc = EXIT_FAILURE;

  // Commented-out JSON is just comment text.
  static const struct jsn_exp commented_out[] = {
    JSN_T (xtree_json_token_LBRACE,  "{",                    0, 0),
    JSN_T (xtree_json_token_COMMENT, "// \"a\": 1, \"b\": [", 1, 2),
    JSN_T (xtree_json_token_RBRACE,  "}",                    2, 0),
  };
  if (!jsn_run ("{\n  // \"a\": 1, \"b\": [\n}", commented_out,
                JSN_N (commented_out)))
    rc = EXIT_FAILURE;

  return rc;
}

// name: test_json_scan_error_unterminated_string
int test_json_scan_error_unterminated_string (void)
{
  // The only way scanning fails. The ERROR token is positioned at the end
  // of the input, where the closing quote was expected. Strings may span
  // lines, so a newline does not end an unterminated string.
  static const struct jsn_bad_case cases[] = {
    { "\"", 0, 1 },
    { "\"abc", 0, 4 },
    { "\"abc\\", 0, 5 },             /* input ends after a backslash */
    { "\"abc\\\"", 0, 6 },           /* the only closing quote is escaped */
    { "\"\\u12", 0, 5 },
    { "\"// comment", 0, 11 },       /* not a comment inside a string */
    { "\"abc\n", 1, 0 },
    { "\"a\nb", 1, 1 },
    { "\"a\r\nb", 1, 1 },
    { "\"\n\n", 2, 0 },
    { "\"line one\nline two", 1, 8 },
  };
  return jsn_run_bad_cases (cases, JSN_N (cases));
}

// name: test_json_scan_error_after_tokens
int test_json_scan_error_after_tokens (void)
{
  // Valid tokens are returned (with correct positions) up to the point
  // of the error.
  int rc = EXIT_SUCCESS;

  static const struct jsn_exp in_object[] = {
    JSN_T (xtree_json_token_LBRACE, "{",     0, 0),
    JSN_T (xtree_json_token_STRING, "\"a\"", 0, 1),
    JSN_T (xtree_json_token_COLON,  ":",     0, 4),
    JSN_T (xtree_json_token_ERROR,  "",      0, 7),
  };
  if (!jsn_run ("{\"a\":\"b", in_object, JSN_N (in_object)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp multi_line[] = {
    JSN_T (xtree_json_token_LBRACKET, "[", 0, 0),
    JSN_T (xtree_json_token_NUMBER,   "1", 0, 1),
    JSN_T (xtree_json_token_COMMA,    ",", 0, 2),
    JSN_T (xtree_json_token_ERROR,    "",  2, 3),
  };
  if (!jsn_run ("[1,\n\"abc\ndef", multi_line, JSN_N (multi_line)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp after_symbol[] = {
    JSN_T (xtree_json_token_SYMBOL, "x", 0, 0),
    JSN_T (xtree_json_token_ERROR,  "",  0, 3),
  };
  if (!jsn_run ("x \"", after_symbol, JSN_N (after_symbol)))
    rc = EXIT_FAILURE;

  static const struct jsn_exp after_comment[] = {
    JSN_T (xtree_json_token_COMMENT, "// c", 0, 0),
    JSN_T (xtree_json_token_ERROR,   "",     1, 1),
  };
  if (!jsn_run ("// c\n\"", after_comment, JSN_N (after_comment)))
    rc = EXIT_FAILURE;

  return rc;
}

// name: test_json_scan_long_symbol_and_comment
int test_json_scan_long_symbol_and_comment (void)
{
  int ret = EXIT_FAILURE;
  const size_t len = 100000;
  char *sym = malloc (len + 1);
  char *com = malloc (len + 3);

  if (!sym || !com) {
    PERROR ("Failed to allocate test data\n");
    goto cleanup;
  }

  for (size_t i=0; i<len; i++)
    sym[i] = (char)('A' + (i % 26));
  sym[len] = '\0';

  com[0] = '/';
  com[1] = '/';
  for (size_t i=0; i<len; i++)
    com[i + 2] = (char)('a' + (i % 26));
  com[len + 2] = '\0';

  struct jsn_exp e = JSN_T (xtree_json_token_SYMBOL, sym, 0, 0);
  if (!jsn_run (sym, &e, 1))
    goto cleanup;
  e.id = xtree_json_token_COMMENT;
  e.text = com;
  if (!jsn_run (com, &e, 1))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  free (sym);
  free (com);
  return ret;
}

// name: test_json_scan_null_reader
int test_json_scan_null_reader (void)
{
  // A NULL reader must be handled without crashing. What is returned is
  // up to the library: either NULL or an ERROR token.
  int ret = EXIT_FAILURE;
  xtree_scan_token_t *tok = xtree_json_scan (NULL);

  if (tok && xtree_scan_token_id (tok) != xtree_json_token_ERROR) {
    PERROR ("Scanning a NULL reader returned a [%s] token\n",
            jsn_idname (xtree_scan_token_id (tok)));
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_scan_token_free (&tok);
  return ret;
}

// name: test_json_scan_tokens_independent
int test_json_scan_tokens_independent (void)
{
  // Tokens own their data: freeing one must not disturb another, and
  // both must survive the reader being freed.
  int ret = EXIT_FAILURE;
  xtree_reader_t *reader = xtree_reader_str (JSN_NAME, "\"a\",");
  xtree_scan_token_t *t1 = NULL;
  xtree_scan_token_t *t2 = NULL;

  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  t1 = xtree_json_scan (reader);
  t2 = xtree_json_scan (reader);
  if (!t1 || !t2) {
    PERROR ("Failed to scan two tokens\n");
    goto cleanup;
  }

  xtree_reader_free (&reader);

  const char *text = xtree_scan_token_text (t1);
  if (xtree_scan_token_id (t1) != xtree_json_token_STRING ||
      !text || strcmp (text, "\"a\"") != 0) {
    PERROR ("First token is wrong after the reader was freed\n");
    goto cleanup;
  }

  xtree_scan_token_free (&t1);
  t1 = NULL;

  text = xtree_scan_token_text (t2);
  if (xtree_scan_token_id (t2) != xtree_json_token_COMMA ||
      !text || strcmp (text, ",") != 0) {
    PERROR ("Second token is wrong after the first was deleted\n");
    goto cleanup;
  }

  ret = EXIT_SUCCESS;

cleanup:
  xtree_scan_token_free (&t1);
  xtree_scan_token_free (&t2);
  xtree_reader_free (&reader);
  return ret;
}

// name: test_json_scan_long_string
int test_json_scan_long_string (void)
{
  // Much longer than any plausible internal buffer.
  int ret = EXIT_FAILURE;
  const size_t len = 100000;
  char *buf = malloc (len + 3);

  if (!buf) {
    PERROR ("Failed to allocate test data\n");
    return EXIT_FAILURE;
  }

  buf[0] = '"';
  for (size_t i=0; i<len; i++)
    buf[i + 1] = (char)('a' + (i % 26));
  buf[len + 1] = '"';
  buf[len + 2] = '\0';

  struct jsn_exp e = JSN_T (xtree_json_token_STRING, buf, 0, 0);
  if (!jsn_run (buf, &e, 1))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  free (buf);
  return ret;
}

// name: test_json_scan_long_number
int test_json_scan_long_number (void)
{
  int ret = EXIT_FAILURE;
  const size_t ndigits = 5000;
  static const char tail[] = ".5e+10";
  char *buf = malloc (1 + ndigits + sizeof tail);

  if (!buf) {
    PERROR ("Failed to allocate test data\n");
    return EXIT_FAILURE;
  }

  buf[0] = '1';
  for (size_t i=0; i<ndigits; i++)
    buf[i + 1] = (char)('0' + (i % 10));
  memcpy (buf + 1 + ndigits, tail, sizeof tail);   /* includes the NUL */

  struct jsn_exp e = JSN_T (xtree_json_token_NUMBER, buf, 0, 0);
  if (!jsn_run (buf, &e, 1))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  free (buf);
  return ret;
}

// name: test_json_scan_many_tokens
int test_json_scan_many_tokens (void)
{
  // "[0,0,0,...,0]": 2 + count numbers + (count - 1) commas tokens.
  int ret = EXIT_FAILURE;
  const size_t count = 5000;
  xtree_reader_t *reader = NULL;
  char *buf = malloc (2 * count + 2);
  size_t n = 0;

  if (!buf) {
    PERROR ("Failed to allocate test data\n");
    return EXIT_FAILURE;
  }

  buf[n++] = '[';
  for (size_t i=0; i<count; i++) {
    if (i)
      buf[n++] = ',';
    buf[n++] = '0';
  }
  buf[n++] = ']';
  buf[n] = '\0';

  reader = xtree_reader_str (JSN_NAME, buf);
  if (!reader) {
    PERROR ("Failed to create reader\n");
    goto cleanup;
  }

  size_t line0 = xtree_reader_line (reader);
  size_t cpos0 = xtree_reader_cpos (reader);

  if (!jsn_check_token (reader, xtree_json_token_LBRACKET, "[",
                        line0, cpos0))
    goto cleanup;
  for (size_t i=0; i<count; i++) {
    if (!jsn_check_token (reader, xtree_json_token_NUMBER, "0",
                          line0, cpos0 + 1 + 2 * i)) {
      PERROR ("Number %zu was wrong\n", i);
      goto cleanup;
    }
    if (i + 1 < count &&
        !jsn_check_token (reader, xtree_json_token_COMMA, ",",
                          line0, cpos0 + 2 + 2 * i)) {
      PERROR ("Comma %zu was wrong\n", i);
      goto cleanup;
    }
  }
  if (!jsn_check_token (reader, xtree_json_token_RBRACKET, "]",
                        line0, cpos0 + 2 * count))
    goto cleanup;
  if (!jsn_check_eof (reader))
    goto cleanup;

  ret = EXIT_SUCCESS;

cleanup:
  xtree_reader_free (&reader);
  free (buf);
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
    TEST (test_reader_high_bytes),
    TEST (test_reader_unget_after_eof),
    TEST (test_reader_input_copied),
    TEST (test_reader_independent_readers),
    TEST (test_reader_error_none_on_success),
    TEST (test_reader_error_query_idempotent),
    TEST (test_reader_error_null_reader),
    TEST (test_reader_roundtrip),
    /* FILE tests */
    TEST (test_reader_file_basic),
    TEST (test_reader_file_large),
    TEST (test_reader_file_null_input),

#if 0
    /* Tests for xtree_json_scan() */
    TEST (test_json_scan_eof_empty),
    TEST (test_json_scan_eof_whitespace_only),
    TEST (test_json_scan_punctuation_each),
    TEST (test_json_scan_punctuation_sequence),
    TEST (test_json_scan_punctuation_whitespace_separated),
    TEST (test_json_scan_grammar_agnostic),
    TEST (test_json_scan_literals_each),
    TEST (test_json_scan_literals_sequence),
    TEST (test_json_scan_literals_in_structure),
    TEST (test_json_scan_strings_valid),
    TEST (test_json_scan_strings_sequence),
    TEST (test_json_scan_numbers_valid),
    TEST (test_json_scan_numbers_delimited),
    TEST (test_json_scan_whitespace_kinds),
    TEST (test_json_scan_full_document),
    TEST (test_json_scan_token_filename),
    TEST (test_json_scan_string_escapes_not_decoded),
    TEST (test_json_scan_strings_multiline),
    TEST (test_json_scan_symbols_single),
    TEST (test_json_scan_symbols_delimited),
    TEST (test_json_scan_special_values),
    TEST (test_json_scan_comments_single),
    TEST (test_json_scan_comments_in_context),
    TEST (test_json_scan_error_unterminated_string),
    TEST (test_json_scan_error_after_tokens),
    TEST (test_json_scan_null_reader),
    TEST (test_json_scan_tokens_independent),
    TEST (test_json_scan_long_string),
    TEST (test_json_scan_long_number),
    TEST (test_json_scan_many_tokens),
    TEST (test_json_scan_long_symbol_and_comment),
#endif
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
