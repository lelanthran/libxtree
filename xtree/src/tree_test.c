#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Small test program to test the xtree library */
#include "xtree.h"

static int basic_test (void)
{
  int ret = EXIT_FAILURE;

  xtree_node_t *rootnode = xtree_node_new (NULL, xtree_node_type_LIST);
  xtree_node_t *tmpnode = NULL;

  if (!rootnode) {
    XTREE_ERROR ("Failed to create root node\n");
    goto cleanup;
  }

  if (!(xtree_node_attr_new (rootnode, "n1", "v1"))) {
    XTREE_ERROR ("Failed to add first attrs to rootnode\n");
    goto cleanup;
  }

  if (!(xtree_node_attr_value_set (rootnode, "n1", "v-two"))) {
    XTREE_ERROR ("Failed to reset the name of the attribute\n");
    goto cleanup;
  }

  const char *tmp = NULL;
  if (!(tmp = xtree_node_attr_value_get1 (rootnode, "n1"))) {
    XTREE_ERROR ("Failed to retrieve the node attribute value\n");
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

    printf ("type: %i\n", type);
    if (!(tmpnode = xtree_node_new (rootnode, type))) {
      XTREE_ERROR ("Failed to create childnode %zu\n", i);
      goto cleanup;
    }
    const char *set = NULL;
    switch (type) {
      case xtree_node_type_ATOM:
        set = xtree_node_value_set (tmpnode, nvalue);
        break;
      case xtree_node_type_LIST:
        set = xtree_node_attr_new (tmpnode, "_tag",  nname);
        break;
    }
    printf ("set: [%s]\n", set);
  }

  xtree_node_dump (rootnode, NULL, 1);
  ret = EXIT_SUCCESS;
cleanup:
  xtree_node_free (&tmpnode);
  xtree_node_free (&rootnode);
  return ret;
}

int main (void)
{
  static const struct {
    const char *name;
    int (*fptr) (void);
  } tests[] = {
    { "basic_test", basic_test },
  };
  static const size_t ntests = sizeof tests / sizeof tests[0];

  printf ("testing tree library\n");
  for (size_t i=0; i<ntests; i++) {
    int rc = tests[i].fptr ();
    printf ("[%s]: %s\n", tests[i].name, rc ? "failed" : "passed");
  }
  return 0;
}

